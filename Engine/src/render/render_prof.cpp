#include <render/render_prof.hpp>

#if ENGINE_DEBUG_GPUPROF

#include <render/renderer.hpp>
#include <render/buffer.hpp>
#include <render/commandqueue.hpp>
#include <system/eventProf.hpp>
#include <system/eventProfLane.hpp>
#include <system/logger.hpp>

#include <d3dx12.h>
#include <format>

namespace render
{
	namespace gpuProf
	{
		constexpr uint GPUPROF_MAX_EVENTS_PER_QUEUE = 256;

		enum GPUPROF_QUEUE
		{
			GPUPROF_QUEUE_GRAPHIC = 0,
			GPUPROF_QUEUE_COMPUTE = 1,
			GPUPROF_QUEUE_COPY = 2,
			GPUPROF_QUEUE_COUNT = 3,
		};

		class gpuProfiler
		{
		public:
			bool init();
			void close();
			void endFrame();
			int beginEvent(ID3D12GraphicsCommandList* cmdList, prof::EVENT_INDEX nameID);
			void endEvent(ID3D12GraphicsCommandList* cmdList, int eventIndex);

			uint                      laneCount() const;
			const prof::profLaneView* laneView(uint lane) const;
			const float*              eventHistoryFor(prof::EVENT_INDEX id) const;
			uint                      historyOffset() const;

			uint                           eventCatalogCountValue() const;
			const prof::profEventInfoView* eventCatalogEntry(prof::EVENT_INDEX id) const;
			uint                           catalogOrderCountValue() const;
			prof::EVENT_INDEX              catalogOrderAt(uint i) const;

		private:
			Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeaps[GPUPROF_QUEUE_COUNT];
			bool copyQueueSupported;

			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> resolveAlloc[GPUPROF_QUEUE_COUNT];
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> resolveList[GPUPROF_QUEUE_COUNT];

			buffer* readback;

			UINT64 frequency[GPUPROF_QUEUE_COUNT];

			prof::profLane lanes[GPUPROF_QUEUE_COUNT];

			bool frameActive;
			bool enabled;
			UINT64 frameIndex;

			bool warnedCopyUnsupported;
			bool warnedRegionFull[GPUPROF_QUEUE_COUNT];
			bool warnedMismatchedStack;
			bool warnedNegativeTick;

			prof::profEventView snapshotEvents[GPUPROF_QUEUE_COUNT][GPUPROF_MAX_EVENTS_PER_QUEUE];
			prof::profLaneView snapshotLanes[GPUPROF_QUEUE_COUNT];
			const char* queueLabels[GPUPROF_QUEUE_COUNT];
			float laneTotalHistory[GPUPROF_QUEUE_COUNT][prof::PROF_HISTORY_FRAMES];
			float eventHistory[prof::EVENT_CAPACITY][prof::PROF_HISTORY_FRAMES];
			uint historyHead;

			prof::profEventInfoView eventCatalog[prof::EVENT_CAPACITY];
			uint eventCatalogCount;
			prof::EVENT_INDEX catalogOrder[prof::EVENT_CAPACITY];
			uint catalogOrderCount;

			GPUPROF_QUEUE getQueueFromListType(D3D12_COMMAND_LIST_TYPE type);
			void resolveAllPending();
			void publishSnapshot();
		};

		gpuProfiler g_gpuProf;

		GPUPROF_QUEUE gpuProfiler::getQueueFromListType(D3D12_COMMAND_LIST_TYPE type)
		{
			switch (type)
			{
			case D3D12_COMMAND_LIST_TYPE_DIRECT:
				return GPUPROF_QUEUE_GRAPHIC;
			case D3D12_COMMAND_LIST_TYPE_COMPUTE:
				return GPUPROF_QUEUE_COMPUTE;
			case D3D12_COMMAND_LIST_TYPE_COPY:
				return GPUPROF_QUEUE_COPY;
			default:
				return GPUPROF_QUEUE_COUNT;
			}
		}

		bool gpuProfiler::init()
		{
			copyQueueSupported = false;
			frameActive = true;
			enabled = true;
			frameIndex = 0;
			readback = nullptr;
			historyHead = 0;

			warnedCopyUnsupported = false;
			warnedMismatchedStack = false;
			warnedNegativeTick = false;
			for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
			{
				frequency[i] = 0;
				warnedRegionFull[i] = false;
				prof::laneReset(lanes[i]);
			}

			queueLabels[GPUPROF_QUEUE_GRAPHIC] = "Graphic";
			queueLabels[GPUPROF_QUEUE_COMPUTE] = "Compute";
			queueLabels[GPUPROF_QUEUE_COPY] = "Copy";

			for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
			{
				snapshotLanes[i] = {};
				for (uint j = 0; j < GPUPROF_MAX_EVENTS_PER_QUEUE; ++j)
				{
					snapshotEvents[i][j] = {};
				}
				for (uint j = 0; j < prof::PROF_HISTORY_FRAMES; ++j)
				{
					laneTotalHistory[i][j] = 0.0f;
				}
			}
			for (uint i = 0; i < prof::EVENT_CAPACITY; ++i)
			{
				for (uint j = 0; j < prof::PROF_HISTORY_FRAMES; ++j)
				{
					eventHistory[i][j] = 0.0f;
				}
				eventCatalog[i] = {};
				eventCatalog[i].nameID = prof::EVENT_INVALID;
				eventCatalog[i].lane = 0;
				eventCatalog[i].depth = 0;
				eventCatalog[i].activeThisFrame = false;
			}
			eventCatalogCount = 0;
			catalogOrderCount = 0;

			D3D12_QUERY_HEAP_DESC heapDesc = {};
			heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
			heapDesc.Count = GPUPROF_MAX_EVENTS_PER_QUEUE * 2;
			heapDesc.NodeMask = 0;

			HRESULT hr = e_globRenderer.device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeaps[GPUPROF_QUEUE_GRAPHIC]));
			TC_CONDITIONB(SUCCEEDED(hr), "Failed to create graphics queue timestamp query heap");

			hr = e_globRenderer.device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeaps[GPUPROF_QUEUE_COMPUTE]));
			TC_CONDITIONB(SUCCEEDED(hr), "Failed to create compute queue timestamp query heap");

			D3D12_FEATURE_DATA_D3D12_OPTIONS3 opts3 = {};
			hr = e_globRenderer.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &opts3, sizeof(opts3));
			if (SUCCEEDED(hr) && opts3.CopyQueueTimestampQueriesSupported)
			{
				copyQueueSupported = true;

				D3D12_QUERY_HEAP_DESC copyHeapDesc = {};
				copyHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP;
				copyHeapDesc.Count = GPUPROF_MAX_EVENTS_PER_QUEUE * 2;
				copyHeapDesc.NodeMask = 0;

				hr = e_globRenderer.device->CreateQueryHeap(&copyHeapDesc, IID_PPV_ARGS(&queryHeaps[GPUPROF_QUEUE_COPY]));
				if (FAILED(hr))
				{
					TC_LOG("Failed to create copy-queue timestamp query heap; copy timestamps will be disabled");
					copyQueueSupported = false;
				}
			}

			uint readbackSize = GPUPROF_QUEUE_COUNT * GPUPROF_MAX_EVENTS_PER_QUEUE * 2 * sizeof(UINT64);
			readback = e_globBufAllocator.alloc(nullptr, readbackSize, 1, 0, buf::RESOURCE_READBACK);
			TC_CONDITIONB(readback != nullptr, "Failed to allocate readback buffer for GPU profiler");

			UINT64 graphicsFreq = 0;
			hr = render::getCmdQueue(QUEUE_GRAPHIC)->getQueue()->GetTimestampFrequency(&graphicsFreq);
			TC_CONDITIONB(SUCCEEDED(hr), "Failed to query graphics queue timestamp frequency");
			frequency[GPUPROF_QUEUE_GRAPHIC] = graphicsFreq;

			UINT64 computeFreq = 0;
			hr = render::getCmdQueue(QUEUE_COMPUTE)->getQueue()->GetTimestampFrequency(&computeFreq);
			TC_CONDITIONB(SUCCEEDED(hr), "Failed to query compute queue timestamp frequency");
			frequency[GPUPROF_QUEUE_COMPUTE] = computeFreq;

			if (copyQueueSupported)
			{
				UINT64 copyFreq = 0;
				hr = render::getCmdQueue(QUEUE_COPY)->getQueue()->GetTimestampFrequency(&copyFreq);
				if (FAILED(hr))
				{
					TC_LOG("Failed to query copy-queue timestamp frequency; copy timestamps will be disabled");
					copyQueueSupported = false;
				}
				else
				{
					frequency[GPUPROF_QUEUE_COPY] = copyFreq;
				}
			}

			for (uint region = 0; region < GPUPROF_QUEUE_COUNT; ++region)
			{
				D3D12_COMMAND_LIST_TYPE cmdType;
				if (region == GPUPROF_QUEUE_GRAPHIC)
					cmdType = D3D12_COMMAND_LIST_TYPE_DIRECT;
				else if (region == GPUPROF_QUEUE_COMPUTE)
					cmdType = D3D12_COMMAND_LIST_TYPE_COMPUTE;
				else
					cmdType = D3D12_COMMAND_LIST_TYPE_COPY;

				if (region == GPUPROF_QUEUE_COPY && !copyQueueSupported)
					continue;

				hr = e_globRenderer.device->CreateCommandAllocator(cmdType, IID_PPV_ARGS(&resolveAlloc[region]));
				TC_CONDITIONB(SUCCEEDED(hr), "Failed to create resolve command allocator");

				hr = e_globRenderer.device->CreateCommandList(0, cmdType, resolveAlloc[region].Get(), nullptr, IID_PPV_ARGS(&resolveList[region]));
				TC_CONDITIONB(SUCCEEDED(hr), "Failed to create resolve command list");

				hr = resolveList[region]->Close();
				TC_CONDITIONB(SUCCEEDED(hr), "Failed to close resolve command list");
			}

#if ENGINE_DEBUG_GPUPROF_LOG
			TC_LOG(std::format("GPU Profiler initialized; copy timestamps supported: {}", copyQueueSupported).c_str());
			TC_LOG("=== Registered GPU Events ===");
			for (int i = 0; i < prof::getEventCount(); ++i)
			{
				TC_LOG(std::format("[{}] {}", i, prof::getEventName(static_cast<prof::EVENT_INDEX>(i))).c_str());
			}
#endif

			return true;
		}

		void gpuProfiler::close()
		{
			for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
			{
				queryHeaps[i] = nullptr;
			}

			for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
			{
				resolveAlloc[i] = nullptr;
				resolveList[i] = nullptr;
			}

			readback = nullptr;
		}

		int gpuProfiler::beginEvent(ID3D12GraphicsCommandList* cmdList, prof::EVENT_INDEX nameID)
		{
			if (!enabled || !frameActive || !cmdList)
				return -1;

			D3D12_COMMAND_LIST_TYPE listType = cmdList->GetType();
			GPUPROF_QUEUE region = getQueueFromListType(listType);

			if (region == GPUPROF_QUEUE_COUNT || !queryHeaps[region])
				return -1;

			if (region == GPUPROF_QUEUE_COPY && !copyQueueSupported)
				return -1;

			if (lanes[region].recordedCount >= GPUPROF_MAX_EVENTS_PER_QUEUE)
			{
				if (!warnedRegionFull[region])
				{
					TC_LOG(std::format("GPU Profiler: {} queue event buffer full (max {} events)",
						region == GPUPROF_QUEUE_GRAPHIC ? "graphic" : (region == GPUPROF_QUEUE_COMPUTE ? "compute" : "copy"),
						GPUPROF_MAX_EVENTS_PER_QUEUE).c_str());
					warnedRegionFull[region] = true;
				}
				return -1;
			}

			prof::profLane& lane = lanes[region];
			uint localSlot = lanes[region].recordedCount * 2;

			int eventIndex = prof::laneBegin(lane, nameID, std::format("GPU-{}", static_cast<int>(region)).c_str());
			if (eventIndex < 0)
				return -1;

			lane.pendingEvents[eventIndex].slot = localSlot;
			cmdList->EndQuery(queryHeaps[region].Get(), D3D12_QUERY_TYPE_TIMESTAMP, localSlot);

			return eventIndex;
		}

		void gpuProfiler::endEvent(ID3D12GraphicsCommandList* cmdList, int eventIndex)
		{
			if (!frameActive || !cmdList)
				return;

			D3D12_COMMAND_LIST_TYPE listType = cmdList->GetType();
			GPUPROF_QUEUE region = getQueueFromListType(listType);

			if (region == GPUPROF_QUEUE_COUNT || !queryHeaps[region] || eventIndex < 0 || eventIndex >= static_cast<int>(lanes[region].pendingEvents.size()))
				return;

			prof::profLane& lane = lanes[region];
			prof::profEvent& event = lane.pendingEvents[eventIndex];

			uint endSlot = event.slot + 1;
			cmdList->EndQuery(queryHeaps[region].Get(), D3D12_QUERY_TYPE_TIMESTAMP, endSlot);

			bool warnedMismatch = false;
			prof::laneEnd(lane, eventIndex, warnedMismatch);
			if (warnedMismatch && !warnedMismatchedStack)
			{
				TC_LOG("GPU Profiler: event stack mismatch (scope nesting error)");
				warnedMismatchedStack = true;
			}
		}

		void gpuProfiler::resolveAllPending()
		{
			for (uint region = 0; region < GPUPROF_QUEUE_COUNT; ++region)
			{
				if (!enabled)
					continue;
				prof::profLane& lane = lanes[region];
				if (lanes[region].recordedCount == lanes[region].resolvedCount)
					continue;
				if (region == GPUPROF_QUEUE_COPY && !copyQueueSupported)
					continue;
				if (!lane.openStack.empty())
				{
					if (!warnedMismatchedStack)
					{
						TC_LOG("GPU Profiler: resolveAllPending called with open events (should not happen)");
						warnedMismatchedStack = true;
					}
					continue;
				}

				resolveAlloc[region]->Reset();
				resolveList[region]->Reset(resolveAlloc[region].Get(), nullptr);

				ID3D12QueryHeap* heap = queryHeaps[region].Get();
				uint readbackBase = region * GPUPROF_MAX_EVENTS_PER_QUEUE * 2;

				uint numQueriesToResolve = (lanes[region].recordedCount - lanes[region].resolvedCount) * 2;
				UINT resolveStartSlot = lanes[region].resolvedCount * 2;
				UINT64 readbackByteOffset = (readbackBase + lanes[region].resolvedCount * 2) * sizeof(UINT64);

				resolveList[region]->ResolveQueryData(
					heap,
					D3D12_QUERY_TYPE_TIMESTAMP,
					resolveStartSlot,
					numQueriesToResolve,
					readback->getResource(),
					readbackByteOffset
				);

				lanes[region].resolvedCount = lanes[region].recordedCount;

				render::getCmdQueue((QUEUE_INDEX)region)->execute({ resolveList[region] });
				render::getCmdQueue((QUEUE_INDEX)region)->flush();
			}
		}

		void gpuProfiler::publishSnapshot()
		{
			historyHead = (historyHead + 1) % prof::PROF_HISTORY_FRAMES;

			for (uint i = 0; i < prof::EVENT_CAPACITY; ++i)
			{
				eventHistory[i][historyHead] = 0.0f;
				eventCatalog[i].activeThisFrame = false;
			}

			catalogOrderCount = 0;

			for (uint queueIdx = 0; queueIdx < GPUPROF_QUEUE_COUNT; ++queueIdx)
			{
				prof::profLane& lane = lanes[queueIdx];
				uint queueFirstOrderIndex = catalogOrderCount;

				uint eventCount = (std::min)(static_cast<uint>(lane.lastFrameEvents.size()), GPUPROF_MAX_EVENTS_PER_QUEUE);
				for (uint eventIdx = 0; eventIdx < eventCount; ++eventIdx)
				{
					const prof::profEvent& event = lane.lastFrameEvents[eventIdx];
					snapshotEvents[queueIdx][eventIdx] = {
						prof::getEventName(event.nameID),
						event.depth,
						static_cast<float>(event.timeMs),
						event.nameID
					};

					if (event.nameID >= 0 && event.nameID < prof::EVENT_CAPACITY)
					{
						eventHistory[event.nameID][historyHead] += static_cast<float>(event.timeMs);

						if (!eventCatalog[event.nameID].activeThisFrame)
						{
							uint clampedDepth = event.depth;
							if (catalogOrderCount > queueFirstOrderIndex)
							{
								uint prevEventId = static_cast<uint>(catalogOrder[catalogOrderCount - 1]);
								uint prevDepth = eventCatalog[prevEventId].depth;
								if (clampedDepth > prevDepth + 1)
									clampedDepth = prevDepth + 1;
							}
							else
							{
								if (clampedDepth > 0)
									clampedDepth = 0;
							}

							if (catalogOrderCount < prof::EVENT_CAPACITY)
							{
								catalogOrder[catalogOrderCount] = static_cast<prof::EVENT_INDEX>(event.nameID);
								++catalogOrderCount;
							}

							eventCatalog[event.nameID].depth = clampedDepth;
						}

						eventCatalog[event.nameID].nameID = event.nameID;
						eventCatalog[event.nameID].lane = queueIdx;
						eventCatalog[event.nameID].activeThisFrame = true;
					}
				}

				snapshotLanes[queueIdx].label = queueLabels[queueIdx];
				snapshotLanes[queueIdx].totalMs = static_cast<float>(lane.totalMs);
				snapshotLanes[queueIdx].events = snapshotEvents[queueIdx];
				snapshotLanes[queueIdx].eventCount = eventCount;
				snapshotLanes[queueIdx].totalHistory = laneTotalHistory[queueIdx];

				laneTotalHistory[queueIdx][historyHead] = static_cast<float>(lane.totalMs);
			}

			uint totalEventCount = (std::min)(static_cast<uint>(prof::getEventCount()), static_cast<uint>(prof::EVENT_CAPACITY));
			for (uint i = 0; i < totalEventCount && catalogOrderCount < prof::EVENT_CAPACITY; ++i)
			{
				if (eventCatalog[i].nameID != prof::EVENT_INVALID && !eventCatalog[i].activeThisFrame)
				{
					eventCatalog[i].depth = 0;
					catalogOrder[catalogOrderCount] = static_cast<prof::EVENT_INDEX>(i);
					++catalogOrderCount;
				}
			}

			eventCatalogCount = (std::min)(static_cast<uint>(prof::getEventCount()), static_cast<uint>(prof::EVENT_CAPACITY));
		}

		void gpuProfiler::endFrame()
		{
			resolveAllPending();

			frameActive = false;

			if (lanes[GPUPROF_QUEUE_GRAPHIC].recordedCount == 0 && lanes[GPUPROF_QUEUE_COMPUTE].recordedCount == 0 &&
				lanes[GPUPROF_QUEUE_COPY].recordedCount == 0)
			{
				publishSnapshot();

				for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
				{
					prof::laneReset(lanes[i]);
				}
				frameActive = true;

				++frameIndex;
				return;
			}

			uint readbackSize = GPUPROF_QUEUE_COUNT * GPUPROF_MAX_EVENTS_PER_QUEUE * 2 * sizeof(UINT64);
			unsigned char* mappedPtr = nullptr;

			if (!readback->mapReadbackBuffer(&mappedPtr, readbackSize))
			{
				TC_LOG("GPU Profiler: failed to map readback buffer");

				publishSnapshot();

				for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
				{
					prof::laneReset(lanes[i]);
				}
				frameActive = true;

				++frameIndex;
				return;
			}

			UINT64* ticks = reinterpret_cast<UINT64*>(mappedPtr);

			for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
			{
				lanes[i].totalMs = 0.0;
				prof::profLane& lane = lanes[i];
				lane.lastFrameEvents = lane.pendingEvents;
			}

			for (uint region = 0; region < GPUPROF_QUEUE_COUNT; ++region)
			{
				uint readbackBase = region * GPUPROF_MAX_EVENTS_PER_QUEUE * 2;
				prof::profLane& lane = lanes[region];

				for (auto& event : lane.lastFrameEvents)
				{
					uint localSlot = event.slot;

					uint beginTickIdx = readbackBase + localSlot;
					uint endTickIdx = readbackBase + localSlot + 1;

					if (beginTickIdx < GPUPROF_QUEUE_COUNT * GPUPROF_MAX_EVENTS_PER_QUEUE * 2 &&
						endTickIdx < GPUPROF_QUEUE_COUNT * GPUPROF_MAX_EVENTS_PER_QUEUE * 2)
					{
						event.beginTick = ticks[beginTickIdx];
						UINT64 endTick = ticks[endTickIdx];

						if (endTick >= event.beginTick)
						{
							event.timeMs = (endTick - event.beginTick) * 1000.0 / frequency[region];
						}
						else
						{
							event.timeMs = 0.0;
							if (!warnedNegativeTick)
							{
								TC_LOG("GPU Profiler: disjoint timestamp detected (endTick < beginTick); clamping to 0");
								warnedNegativeTick = true;
							}
						}
					}

					if (event.depth == 0)
					{
						lanes[region].totalMs += event.timeMs;
					}
				}
			}

			readback->unmapReadbackBuffer();

#if ENGINE_DEBUG_GPUPROF_LOG
			if (frameIndex == 60)
			{
				TC_LOG("=== GPU Profiler Frame 60 ===");
				for (uint regionIdx = 0; regionIdx < GPUPROF_QUEUE_COUNT; ++regionIdx)
				{
					const char* queueLabel = (regionIdx == GPUPROF_QUEUE_GRAPHIC) ? "G" :
						(regionIdx == GPUPROF_QUEUE_COMPUTE) ? "C" : "CP";

					prof::laneDump(lanes[regionIdx], queueLabel);
				}

				TC_LOG(std::format("[GPUProf] Queue Totals: Graphic={:.3f}ms, Compute={:.3f}ms, Copy={:.3f}ms",
					lanes[GPUPROF_QUEUE_GRAPHIC].totalMs,
					lanes[GPUPROF_QUEUE_COMPUTE].totalMs,
					lanes[GPUPROF_QUEUE_COPY].totalMs).c_str());
				TC_LOG("=== End Frame 60 ===");

				frameIndex = 0;
			}
#endif

			publishSnapshot();

			for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
			{
				prof::laneReset(lanes[i]);
			}
			frameActive = true;

			++frameIndex;
		}

		uint gpuProfiler::laneCount() const
		{
			return GPUPROF_QUEUE_COUNT;
		}

		const prof::profLaneView* gpuProfiler::laneView(uint lane) const
		{
			if (lane >= GPUPROF_QUEUE_COUNT)
				return nullptr;
			return &snapshotLanes[lane];
		}

		const float* gpuProfiler::eventHistoryFor(prof::EVENT_INDEX id) const
		{
			if (id < 0 || id >= prof::EVENT_CAPACITY)
				return nullptr;
			return eventHistory[id];
		}

		uint gpuProfiler::historyOffset() const
		{
			return (historyHead + 1) % prof::PROF_HISTORY_FRAMES;
		}

		uint gpuProfiler::eventCatalogCountValue() const
		{
			return eventCatalogCount;
		}

		const prof::profEventInfoView* gpuProfiler::eventCatalogEntry(prof::EVENT_INDEX id) const
		{
			if (id < 0 || static_cast<uint>(id) >= eventCatalogCount)
				return nullptr;
			return &eventCatalog[id];
		}

		uint gpuProfiler::catalogOrderCountValue() const
		{
			return catalogOrderCount;
		}

		prof::EVENT_INDEX gpuProfiler::catalogOrderAt(uint i) const
		{
			if (i >= catalogOrderCount)
				return prof::EVENT_INVALID;
			return catalogOrder[i];
		}
	}

	bool initGPUProfBackend()
	{
		return gpuProf::g_gpuProf.init();
	}

	void closeGPUProfBackend()
	{
		gpuProf::g_gpuProf.close();
	}

	void endGPUProfFrameBackend()
	{
		gpuProf::g_gpuProf.endFrame();
	}

	int beginGPUProfEventBackend(void* cmdList, prof::EVENT_INDEX nameID)
	{
		return gpuProf::g_gpuProf.beginEvent(static_cast<ID3D12GraphicsCommandList*>(cmdList), nameID);
	}

	void endGPUProfEventBackend(void* cmdList, int eventIndex)
	{
		gpuProf::g_gpuProf.endEvent(static_cast<ID3D12GraphicsCommandList*>(cmdList), eventIndex);
	}

	uint getGPULaneCountBackend()
	{
		return gpuProf::GPUPROF_QUEUE_COUNT;
	}

	const prof::profLaneView* getGPULaneViewBackend(uint lane)
	{
		return gpuProf::g_gpuProf.laneView(lane);
	}

	const float* getGPUEventHistoryBackend(prof::EVENT_INDEX eventID)
	{
		return gpuProf::g_gpuProf.eventHistoryFor(eventID);
	}

	uint getGPUHistoryOffsetBackend()
	{
		return gpuProf::g_gpuProf.historyOffset();
	}

	uint getGPUEventCatalogCountBackend()
	{
		return gpuProf::g_gpuProf.eventCatalogCountValue();
	}

	const prof::profEventInfoView* getGPUEventCatalogBackend(prof::EVENT_INDEX id)
	{
		return gpuProf::g_gpuProf.eventCatalogEntry(id);
	}

	uint getGPUEventCatalogOrderCountBackend()
	{
		return gpuProf::g_gpuProf.catalogOrderCountValue();
	}

	prof::EVENT_INDEX getGPUEventCatalogOrderBackend(uint i)
	{
		return gpuProf::g_gpuProf.catalogOrderAt(i);
	}

}  // namespace render

#endif  // ENGINE_DEBUG_GPUPROF
