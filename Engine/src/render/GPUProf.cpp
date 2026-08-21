#include <render/GPUProf.hpp>

#if ENGINE_DEBUG_GPUPROF

#include <render/renderer.hpp>
#include <render/buffer.hpp>
#include <render/commandqueue.hpp>
#include <system/logger.hpp>

#include <d3dx12.h>

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

		struct gpuProfEvent
		{
			GPUEVENT_INDEX nameID;
			uint depth;
			int  parent;
			uint slot;
			UINT64 beginTick;// kept for a future timeline view; not consumed today
			double timeMs;
		};

		class gpuProfiler
		{
		public:
			bool init();
			void close();

			void endFrame();

			int beginEvent(ID3D12GraphicsCommandList* cmdList, GPUEVENT_INDEX nameID);
			void endEvent(ID3D12GraphicsCommandList* cmdList, int eventIndex);

		private:
			Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeaps[GPUPROF_QUEUE_COUNT];
			bool copyQueueSupported;

			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> resolveAlloc[GPUPROF_QUEUE_COUNT];
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> resolveList[GPUPROF_QUEUE_COUNT];

			buffer* readback;

			UINT64 frequency[GPUPROF_QUEUE_COUNT];

			uint recordedCount[GPUPROF_QUEUE_COUNT];
			uint resolvedCount[GPUPROF_QUEUE_COUNT];

			std::vector<int> openStack[GPUPROF_QUEUE_COUNT];
			std::vector<gpuProfEvent> pendingEvents[GPUPROF_QUEUE_COUNT];
			std::vector<gpuProfEvent> lastFrameEvents[GPUPROF_QUEUE_COUNT];

			double queueTotalMs[GPUPROF_QUEUE_COUNT];

			bool frameActive;
			bool enabled;
			UINT64 frameIndex;

			bool warnedCopyUnsupported;
			bool warnedRegionFull[GPUPROF_QUEUE_COUNT];
			bool warnedMismatchedStack;
			bool warnedNegativeTick;

			GPUPROF_QUEUE getQueueFromListType(D3D12_COMMAND_LIST_TYPE type);

			void resolveAllPending();
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

			warnedCopyUnsupported = false;
			warnedMismatchedStack = false;
			warnedNegativeTick = false;
			for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
			{
				recordedCount[i] = 0;
				resolvedCount[i] = 0;
				frequency[i] = 0;
				queueTotalMs[i] = 0.0;
				warnedRegionFull[i] = false;
			}

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
			for (int i = 0; i < render::getGPUEventCount(); ++i)
			{
				TC_LOG(std::format("[{}] {}", i, render::getGPUEventName(static_cast<render::GPUEVENT_INDEX>(i))).c_str());
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

		int gpuProfiler::beginEvent(ID3D12GraphicsCommandList* cmdList, GPUEVENT_INDEX nameID)
		{
			if (!enabled || !frameActive || !cmdList)
				return -1;

			D3D12_COMMAND_LIST_TYPE listType = cmdList->GetType();
			GPUPROF_QUEUE region = getQueueFromListType(listType);

			if (region == GPUPROF_QUEUE_COUNT || !queryHeaps[region])
				return -1;

			if (region == GPUPROF_QUEUE_COPY && !copyQueueSupported)
				return -1;

			if (recordedCount[region] >= GPUPROF_MAX_EVENTS_PER_QUEUE)
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

			uint localSlot = recordedCount[region] * 2;
			gpuProfEvent event = {};
			event.nameID = nameID;
			event.depth = static_cast<uint>(openStack[region].size());
			event.parent = openStack[region].empty() ? -1 : static_cast<int>(openStack[region].back());

			event.slot = localSlot;
			event.beginTick = 0;
			event.timeMs = 0.0;

			int eventIndex = static_cast<int>(pendingEvents[region].size());
			pendingEvents[region].push_back(event);

			cmdList->EndQuery(queryHeaps[region].Get(), D3D12_QUERY_TYPE_TIMESTAMP, localSlot);

			openStack[region].push_back(eventIndex);
			++recordedCount[region];

			return eventIndex;
		}

		void gpuProfiler::endEvent(ID3D12GraphicsCommandList* cmdList, int eventIndex)
		{
			if (!frameActive || !cmdList)
				return;

			D3D12_COMMAND_LIST_TYPE listType = cmdList->GetType();
			GPUPROF_QUEUE region = getQueueFromListType(listType);

			if (region == GPUPROF_QUEUE_COUNT || !queryHeaps[region] || eventIndex < 0 || eventIndex >= static_cast<int>(pendingEvents[region].size()))
				return;

			gpuProfEvent& event = pendingEvents[region][eventIndex];

			uint endSlot = event.slot + 1;
			cmdList->EndQuery(queryHeaps[region].Get(), D3D12_QUERY_TYPE_TIMESTAMP, endSlot);

			if (!openStack[region].empty() && openStack[region].back() == eventIndex)
			{
				openStack[region].pop_back();
			}
			else if (!openStack[region].empty() && !warnedMismatchedStack)
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
				if (recordedCount[region] == resolvedCount[region])
					continue;
				if (region == GPUPROF_QUEUE_COPY && !copyQueueSupported)
					continue;
				if (!openStack[region].empty())
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

				uint numQueriesToResolve = (recordedCount[region] - resolvedCount[region]) * 2;
				UINT resolveStartSlot = resolvedCount[region] * 2;
				UINT64 readbackByteOffset = (readbackBase + resolvedCount[region] * 2) * sizeof(UINT64);

				resolveList[region]->ResolveQueryData(
					heap,
					D3D12_QUERY_TYPE_TIMESTAMP,
					resolveStartSlot,
					numQueriesToResolve,
					readback->getResource(),
					readbackByteOffset
				);

				resolvedCount[region] = recordedCount[region];

				render::getCmdQueue((QUEUE_INDEX)region)->execute({ resolveList[region] });
				render::getCmdQueue((QUEUE_INDEX)region)->flush();
			}
		}

		void gpuProfiler::endFrame()
		{
			resolveAllPending();

			frameActive = false;

			if (recordedCount[GPUPROF_QUEUE_GRAPHIC] == 0 && recordedCount[GPUPROF_QUEUE_COMPUTE] == 0 &&
				recordedCount[GPUPROF_QUEUE_COPY] == 0)
			{
				for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
				{
					lastFrameEvents[i].clear();
					queueTotalMs[i] = 0.0;
					pendingEvents[i].clear();
					openStack[i].clear();
					recordedCount[i] = 0;
					resolvedCount[i] = 0;
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
				for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
				{
					lastFrameEvents[i].clear();
					queueTotalMs[i] = 0.0;
					pendingEvents[i].clear();
					openStack[i].clear();
					recordedCount[i] = 0;
					resolvedCount[i] = 0;
				}
				frameActive = true;

				++frameIndex;
				return;
			}

			UINT64* ticks = reinterpret_cast<UINT64*>(mappedPtr);

			for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
			{
				queueTotalMs[i] = 0.0;
				lastFrameEvents[i] = pendingEvents[i];
			}

			for (uint region = 0; region < GPUPROF_QUEUE_COUNT; ++region)
			{
				uint readbackBase = region * GPUPROF_MAX_EVENTS_PER_QUEUE * 2;

				for (auto& event : lastFrameEvents[region])
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
						queueTotalMs[region] += event.timeMs;
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

					for (const auto& event : lastFrameEvents[regionIdx])
					{
						std::string indent;
						for (uint d = 0; d < event.depth; ++d)
							indent += "  ";

						TC_LOG(std::format("[GPUProf] {}{}  queue={} {:.3f} ms",
							indent, render::getGPUEventName(event.nameID), queueLabel, event.timeMs).c_str());
					}
				}

				TC_LOG(std::format("[GPUProf] Queue Totals: Graphic={:.3f}ms, Compute={:.3f}ms, Copy={:.3f}ms",
					queueTotalMs[GPUPROF_QUEUE_GRAPHIC],
					queueTotalMs[GPUPROF_QUEUE_COMPUTE],
					queueTotalMs[GPUPROF_QUEUE_COPY]).c_str());
				TC_LOG("=== End Frame 60 ===");
			}
#endif

			for (uint i = 0; i < GPUPROF_QUEUE_COUNT; ++i)
			{
				pendingEvents[i].clear();
				openStack[i].clear();
				recordedCount[i] = 0;
				resolvedCount[i] = 0;
			}
			frameActive = true;

			++frameIndex;
		}

	}  // namespace gpuProf

	bool initGPUProf()
	{
		return gpuProf::g_gpuProf.init();
	}

	void closeGPUProf()
	{
		gpuProf::g_gpuProf.close();
	}

	void endGPUProfFrame()
	{
		gpuProf::g_gpuProf.endFrame();
	}

	int beginGPUProfEvent(ID3D12GraphicsCommandList* cmdList, GPUEVENT_INDEX nameID)
	{
		return gpuProf::g_gpuProf.beginEvent(cmdList, nameID);
	}

	void endGPUProfEvent(ID3D12GraphicsCommandList* cmdList, int eventIndex)
	{
		gpuProf::g_gpuProf.endEvent(cmdList, eventIndex);
	}

} // namespace render

#endif // ENGINE_DEBUG_GPUPROF

