#include <rapidobj/rapidobj.hpp>

#include <render/buffer.hpp>
#include <render/commandqueue.hpp>
#include <render/renderer.hpp>
#include <render/render_debug.hpp>
#include <render/mesh.hpp>
#include <system/window.hpp>
#include <system/logger.hpp>
#include <system/jsonhelper.hpp>
#include <system/mathhelper.hpp>
#include <system/gui.hpp>

#include <vector>
#include <format>
#include <charconv>
#include <cstddef>
#include <limits>
#include <string_view>

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXTex.h>

constexpr uint CBVALLIGNMENT = 256;

buffer_allocator e_globBufAllocator;

constexpr uint BUFFER_HEADER_SIZE = ((4 + 4 + 4));
//state + comptr + dataPointer
constexpr uint BUFFER_VAR_SIZE = sizeof(char*) + sizeof(Microsoft::WRL::ComPtr<ID3D12Resource>) + sizeof(D3D12_RESOURCE_STATES);
constexpr uint BUFFER_VIEW_OFFSET = BUFFER_HEADER_SIZE + BUFFER_VAR_SIZE;

TC_STATICASSERT((BUFFER_HEADER_SIZE) == sizeof(buffer_header));
TC_STATICASSERT((BUFFER_VAR_SIZE + BUFFER_HEADER_SIZE) == sizeof(buffer));

namespace meshloadhelper
{
    struct vertindex
    {
        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<uint> indices;
        std::vector<uint> indicesLine;
    };
}

namespace img
{
    void ResourceBarrier(const D3D12_RESOURCE_BARRIER& /*barrier*/)
    {
        //if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
        //{
        //    const D3D12_RESOURCE_TRANSITION_BARRIER& transitionBarrier = barrier.Transition;

        //    {
        //        // If the known final state of the resource is different...
        //        if (transitionBarrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
        //        {
        //            // First transition all of the subresources if they are different than the StateAfter.
        //            D3D12_RESOURCE_BARRIER newBarrier = barrier;
        //            newBarrier.Transition.Subresource = subresourceState.first;
        //            newBarrier.Transition.StateBefore = subresourceState.second;
        //        }
        //        else
        //        {
        //            auto finalState = resourceState.GetSubresourceState(transitionBarrier.Subresource);
        //            if (transitionBarrier.StateAfter != finalState)
        //            {
        //                // Push a new transition barrier with the correct before state.
        //                D3D12_RESOURCE_BARRIER newBarrier = barrier;
        //                newBarrier.Transition.StateBefore = finalState;
        //            }
        //        }
        //    }
        //}
        //else
        //{
        //    // Just push non-transition barriers to the resource barriers array.
        //    m_ResourceBarriers.push_back(barrier);
        //}
    }
}

namespace buf
{
    inline constexpr void TrimLeft(std::string& text)
    {
        size_t index = 0;
        while (index < text.size() && (text[index] == ' ' || text[index] == '\t' || text[index] == ',' || text[index] == ')' || text[index] == '(' || text[index] == '\n' || text[index] == '\r')) ++index;
        
        text.erase(0, index);
    }

    inline bool ParseReals(std::string& text, uint count, float* out)
    {
        uint parsedCount = 0;

        TrimLeft(text);

        while (!text.empty() && parsedCount < count) 
        {
            auto [ptr, rc] = fast_float::from_chars(text.data(), text.data() + text.size(), *out);
            if (rc != std::errc())
            {
                return false;
            }
            
            auto num_parsed = static_cast<size_t>(ptr - text.data());
            text.erase(0, num_parsed);

            TrimLeft(text);

            ++parsedCount;
            ++out;
        }

        TrimLeft(text);

        return parsedCount == count;
    }

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediates;

#if ENGINE_DEBUG_RESOURCEVIEW
    struct resourceViewRecord
    {
        uint             heapIndex;   // render::descriptorHeapIndex value, stored as uint
        uint             heapOffset;
        buf::BUFFER_TYPE viewType;
    };

    struct resourceDebugInfo
    {
        char                     name[64]{};         // formatted display name, written once at creation
        buffer*                  owner = nullptr;    // back-pointer; non-null == live, nullptr == freed
        std::vector<resourceViewRecord> views;    // variable-length descriptor list
        uint                     cpuArenaBytes = 0;
        uint_8                   viewFlags = 0;
        bool                     texture = false;
        bool                     external = false;
        bool                     oneTime = false;

        const char*              file = nullptr;
        uint                     line = 0;
        bool                     valid = false;

        D3D12_RESOURCE_DIMENSION dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN;
        DXGI_FORMAT              format = DXGI_FORMAT_UNKNOWN;
        UINT64                   width = 0;
        UINT                     height = 0;
        UINT16                   mipLevels = 0;
        D3D12_HEAP_TYPE          heapType = D3D12_HEAP_TYPE_DEFAULT;
        UINT64                   gpuBytes = 0;
    };

    static resourceDebugInfo debugInfoTable[BUFFER_MAX_COUNT];
    static uint debugInfoCount = 0;

    const char* getResourceDisplayName(uint bufferId)
    {
        if (bufferId >= BUFFER_MAX_COUNT)
        {
            return "buffer #INVALID";
        }

        const resourceDebugInfo& info = debugInfoTable[bufferId];
        if (!info.valid)
        {
            return "buffer #UNKNOWN";
        }

        return info.name;
    }

    uint getResourceDebugInfoCount()
    {
        return debugInfoCount;
    }

    bool isBufferResource(uint bufferId)
    {
        if (bufferId >= BUFFER_MAX_COUNT)
        {
            return false;
        }

        const resourceDebugInfo& info = debugInfoTable[bufferId];
        return info.valid && info.owner != nullptr && info.dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
    }

    bool isTextureResource(uint bufferId)
    {
        if (bufferId >= BUFFER_MAX_COUNT)
        {
            return false;
        }

        const resourceDebugInfo& info = debugInfoTable[bufferId];
        return info.valid && info.owner != nullptr && info.texture;
    }

    buffer* getResourceOwner(uint bufferId)
    {
        if (bufferId >= BUFFER_MAX_COUNT)
        {
            return nullptr;
        }

        const resourceDebugInfo& info = debugInfoTable[bufferId];
        return info.valid ? info.owner : nullptr;
    }

    UINT64 getResourceWidth(uint bufferId)
    {
        if (bufferId >= BUFFER_MAX_COUNT)
        {
            return 0;
        }

        const resourceDebugInfo& info = debugInfoTable[bufferId];
        return info.valid ? info.width : 0;
    }

    void recordResourceView(buffer* buf, const descriptor& desc, BUFFER_TYPE type)
    {
        if (buf == nullptr)
            return;

        const uint bufferId = buf->getHeader()->packedData.bufferId;
        if (bufferId >= BUFFER_MAX_COUNT)
            return;

        debugInfoTable[bufferId].views.push_back({ (uint)desc.heapIndex, desc.heapOffset, type });
    }

#endif // ENGINE_DEBUG_RESOURCEVIEW

    namespace
    {
        bool parseIntField(std::string_view field, int& out)
        {
            size_t start = 0;
            while (start < field.size() && (field[start] == ' ' || field[start] == '\t' || field[start] == '\r' || field[start] == '\n'))
            {
                ++start;
            }
            if (start < field.size() && field[start] == '+')
            {
                ++start;
            }

            const char* first = field.data() + start;
            const char* last = field.data() + field.size();
            const auto [ptr, ec] = std::from_chars(first, last, out);
            return ec == std::errc() && ptr != first;
        }
    }

    bool parseMeshInfoText(const std::string& text, meshInfoRecord& out)
    {
        out = meshInfoRecord{};

        constexpr std::string_view lodKey = "Number of LODs : ";
        const size_t lodKeyPos = text.find(lodKey);
        if (lodKeyPos == std::string::npos)
        {
            return false;
        }

        std::string_view rest = std::string_view(text).substr(lodKeyPos + lodKey.size());

        int lodNum = 0;
        if (!parseIntField(rest.substr(0, rest.find('\n')), lodNum) || lodNum < 0 || static_cast<uint>(lodNum) > MAX_MESHES_LOD)
        {
            return false;
        }
        out.lodNum = static_cast<uint>(lodNum);

        constexpr std::string_view firstLodKey = "\n0 : ";
        const size_t firstLodPos = rest.find(firstLodKey);
        if (firstLodPos == std::string_view::npos)
        {
            return false;
        }
        rest = rest.substr(firstLodPos + 1);

        out.lods.reserve(out.lodNum);
        for (uint i = 0; i < out.lodNum; ++i)
        {
            const size_t colon = rest.find(" : ");
            if (colon == std::string_view::npos)
            {
                return false;
            }
            int lodIndex = 0;
            if (!parseIntField(rest.substr(0, colon), lodIndex) || lodIndex < 0 || static_cast<uint>(lodIndex) != i)
            {
                return false;
            }
            rest = rest.substr(colon + 3);

            const size_t comma = rest.find(", ");
            if (comma == std::string_view::npos)
            {
                return false;
            }
            int clusterCount = 0;
            if (!parseIntField(rest.substr(0, comma), clusterCount) || clusterCount < 0)
            {
                return false;
            }
            rest = rest.substr(comma + 2);

            const size_t lineEnd = rest.find('\n');
            if (lineEnd == std::string_view::npos)
            {
                return false;
            }
            int totalIndices = 0;
            if (!parseIntField(rest.substr(0, lineEnd), totalIndices) || totalIndices < 0)
            {
                return false;
            }
            rest = rest.substr(lineEnd + 1);

            out.lods.push_back(meshInfoLod{ static_cast<uint>(clusterCount), static_cast<uint>(totalIndices) });
        }

        const size_t dataStart = rest.find('\n');
        if (dataStart == std::string_view::npos)
        {
            return false;
        }
        std::string numbers(rest.substr(dataStart + 1));

        float halfExtentRaw[3] = {};
        if (!ParseReals(numbers, 3, halfExtentRaw))
        {
            return false;
        }
        for (uint k = 0; k < 3; ++k)
        {
            out.halfExtent[k] = halfExtentRaw[k] / 2.0f;
        }

        constexpr float uintRangeEnd = 4294967296.0f;
        for (const meshInfoLod& lod : out.lods)
        {
            uint totalIndexCount = 0;
            for (uint j = 0; j < lod.clusterCount; ++j)
            {
                float values[11] = {};
                if (!ParseReals(numbers, 11, values) || !(values[0] >= 0.0f) || values[0] >= uintRangeEnd)
                {
                    return false;
                }

                meshInfoCluster cluster{};
                cluster.indexCount = static_cast<uint>(values[0]);
                cluster.sphereCenter[0] = values[1];
                cluster.sphereCenter[1] = values[2];
                cluster.sphereCenter[2] = values[3];
                cluster.sphereRadius = values[4];
                cluster.aabbCenter[0] = values[5];
                cluster.aabbCenter[1] = values[6];
                cluster.aabbCenter[2] = values[7];
                cluster.aabbHalfExtent[0] = values[8];
                cluster.aabbHalfExtent[1] = values[9];
                cluster.aabbHalfExtent[2] = values[10];

                out.clusters.push_back(cluster);
                totalIndexCount += cluster.indexCount;
            }

            if (totalIndexCount != lod.totalIndicesCount)
            {
                return false;
            }
        }

        return true;
    }

    bool loadMeshInfo(std::string fileName, meshData* meshData)
    {
        auto pos = fileName.find(".obj");

        fileName = fileName.substr(0, pos) + ".info";

        char* data = nullptr;
        const int size = rawFileRead(fileName, &data);

        if (size < 0)
        {
            TC_LOG_ERROR(std::format("loadMeshInfo: cannot read '{}'", fileName).c_str());
            return false;
        }

        const std::string stringData = data;
        delete[] data;

        meshInfoRecord info;
        if (!parseMeshInfoText(stringData, info))
        {
            TC_LOG_ERROR(std::format("loadMeshInfo: malformed '{}'", fileName).c_str());
            return false;
        }

        meshData->lodNum = info.lodNum;
        meshData->lodData.resize(info.lodNum);

        size_t clusterIndex = 0;
        for (uint i = 0; i < info.lodNum; ++i)
        {
            const meshInfoLod& lod = info.lods[i];
            meshData->lodData[i].clusterNum = lod.clusterCount;
            meshData->lodData[i].totalIndicesCount = lod.totalIndicesCount;
            for (uint j = 0; j < lod.clusterCount; ++j)
            {
                TC_ASSERT(clusterIndex < info.clusters.size());
                meshData->lodData[i].indexSize.push_back(info.clusters[clusterIndex].indexCount);
                ++clusterIndex;
            }
        }

#if ENGINE_DEBUG_DATATEST
        TC_ASSERT(math::compare_float(info.halfExtent[0], meshData->boundData.halfExtent[msh::AXIS_X]));
        TC_ASSERT(math::compare_float(info.halfExtent[1], meshData->boundData.halfExtent[msh::AXIS_Y]));
        TC_ASSERT(math::compare_float(info.halfExtent[2], meshData->boundData.halfExtent[msh::AXIS_Z]));
#endif // #if ENGINE_DEBUG_DATATEST
        meshData->boundData.halfExtent[msh::AXIS_X] = info.halfExtent[0];
        meshData->boundData.halfExtent[msh::AXIS_Y] = info.halfExtent[1];
        meshData->boundData.halfExtent[msh::AXIS_Z] = info.halfExtent[2];

        for (const meshInfoCluster& cluster : info.clusters)
        {
            spherebound sphere{};
            sphere.center[0] = cluster.sphereCenter[0];
            sphere.center[1] = cluster.sphereCenter[1];
            sphere.center[2] = cluster.sphereCenter[2];
            sphere.radius = cluster.sphereRadius;

            aabbbound aabb{};
            aabb.center[0] = cluster.aabbCenter[0];
            aabb.center[1] = cluster.aabbCenter[1];
            aabb.center[2] = cluster.aabbCenter[2];
            aabb.hExtent[0] = cluster.aabbHalfExtent[0];
            aabb.hExtent[1] = cluster.aabbHalfExtent[1];
            aabb.hExtent[2] = cluster.aabbHalfExtent[2];

            meshData->clusterBounds.push_back({ sphere, aabb });
        }

        return true;
    }

    void loadFiletoMesh(const std::string& fileName, meshData* meshdata, uint id)
    {
        auto result = rapidobj::ParseFile(fileName);

        if (result.error)
        {
            auto errorMsg = result.error.code.message();
            const std::string errorLog = "Failed to load file : " + fileName + '\n' + errorMsg;
            TC_LOG_ERROR(errorLog.c_str());
            return;
        }

        rapidobj::Triangulate(result);

        if (result.error)
        {
            auto errorMsg = result.error.code.message();
            const std::string errorLog = "Failed to triangulate file : " + fileName + '\n' + errorMsg;
            TC_LOG_ERROR(errorLog.c_str());
            return;
        }

        std::vector<uint> indices;

        for (const auto& shape : result.shapes)
        {
            for (auto index : shape.mesh.indices)
            {
                indices.push_back(index.position_index);
            }
        }

        meshdata->boundData.halfExtent[msh::AXIS_X] = 0.0f;
        meshdata->boundData.halfExtent[msh::AXIS_Y] = 0.0f;
        meshdata->boundData.halfExtent[msh::AXIS_Z] = 0.0f;

        //check if the mesh is centered correctly(0,0,0)
#if ENGINE_DEBUG_DATATEST
        float xMin = FLT_MAX;
        float yMin = FLT_MAX;
        float zMin = FLT_MAX;
#endif // #if ENGINE_DEBUG_DATATEST

        for (uint i = 0; i < result.attributes.positions.size(); i += 3)
        {
            const float x = result.attributes.positions[i + 0];
            const float y = result.attributes.positions[i + 1];
            const float z = result.attributes.positions[i + 2];

#if ENGINE_DEBUG_DATATEST
            meshdata->boundData.halfExtent[msh::AXIS_X] = std::max(meshdata->boundData.halfExtent[msh::AXIS_X], x);
            meshdata->boundData.halfExtent[msh::AXIS_Y] = std::max(meshdata->boundData.halfExtent[msh::AXIS_Y], y);
            meshdata->boundData.halfExtent[msh::AXIS_Z] = std::max(meshdata->boundData.halfExtent[msh::AXIS_Z], z);

            xMin = std::min(xMin, x);
            yMin = std::min(yMin, y);
            zMin = std::min(zMin, z);
        }

        math::compare_float(xMin + meshdata->boundData.halfExtent[msh::AXIS_X], 0.0f);
        math::compare_float(yMin + meshdata->boundData.halfExtent[msh::AXIS_Y], 0.0f);
        math::compare_float(zMin + meshdata->boundData.halfExtent[msh::AXIS_Z], 0.0f);
#else // #if ENGINE_DEBUG_DATATEST
        }
#endif // #else // #if ENGINE_DEBUG_DATATEST

        buffer* vbs = e_globBufAllocator.alloc(reinterpret_cast<char*>(result.attributes.positions.data()), static_cast<uint>(sizeof(float) * result.attributes.positions.size()), 3,
            GBF_NONE, buf::RESOURCE_ONETIME | buf::RESOURCE_UPLOAD);
        buffer* norm = nullptr;
        //make sure that the model file contains normal data and it's vertex normal not face normal
        if (result.attributes.normals.size() > 0)
        {
            norm = e_globBufAllocator.alloc(reinterpret_cast<char*>(result.attributes.normals.data()), static_cast<uint>(sizeof(float) * result.attributes.normals.size()), 3,
                GBF_NONE, buf::RESOURCE_ONETIME | buf::RESOURCE_UPLOAD);
            TC_ASSERT(result.attributes.normals.size() == result.attributes.positions.size());
        }
        else
        {
            norm = nullptr;
        }
        buffer* idx = e_globBufAllocator.alloc(reinterpret_cast<char*>(indices.data()), static_cast<uint>(sizeof(uint) * indices.size()), 1,
            GBF_NONE, buf::RESOURCE_ONETIME | buf::RESOURCE_UPLOAD);

        //TODO : not support now
        //meshdata->idxLine = createIndexBuffer(result.shapes[0].lines.indices.data(), static_cast<uint>(sizeof(uint) * result.shapes[0].lines.indices.size()));

        if (!loadMeshInfo(fileName, meshdata))
        {
            return;
        }

        e_globRenderer.uploadMeshToUB(vbs, norm, idx, meshdata, id, 0);
    }

    void uploadLoadedMesh(meshData* meshdata, float* p, float* n, uint* i, uint vNum, uint iNum, uint id)
    {
        buffer* vbs = e_globBufAllocator.alloc(reinterpret_cast<char*>(p), static_cast<uint>(sizeof(float) * 3 * vNum), 3,
            buf::GBF_SRV, buf::RESOURCE_ONETIME | buf::RESOURCE_UPLOAD);
        buffer* norm = e_globBufAllocator.alloc(reinterpret_cast<char*>(n), static_cast<uint>(sizeof(float) * 3 * vNum), 3,
            buf::GBF_SRV, buf::RESOURCE_ONETIME | buf::RESOURCE_UPLOAD);
        buffer* idx = e_globBufAllocator.alloc(reinterpret_cast<char*>(i), static_cast<uint>(sizeof(uint) * iNum), 1,
            buf::GBF_SRV, buf::RESOURCE_ONETIME | buf::RESOURCE_UPLOAD);

        e_globRenderer.uploadMeshToUB(vbs, norm, idx, meshdata, id, 0);
    }

    buffer* loadTextureFromFile(const std::wstring& filename, bool mip)
    {
        DirectX::TexMetadata metadata{};
        DirectX::ScratchImage scratchImage;
        DirectX::ScratchImage mipImage;

        const HRESULT loadResult = DirectX::LoadFromHDRFile(filename.c_str(), &metadata, scratchImage);
        if (FAILED(loadResult))
        {
            TC_LOG_ERROR("loadTextureFromFile: LoadFromHDRFile failed");
            return nullptr;
        }

        //we assume there will be only one image from one file
        const DirectX::Image* image = scratchImage.GetImages();
        if (image == nullptr || scratchImage.GetImageCount() == 0)
        {
            TC_LOG_ERROR("loadTextureFromFile: no image data after load");
            return nullptr;
        }

        const uint maxDim = std::max(static_cast<uint>(image->width), static_cast<uint>(image->height));

        uint mipSize = 1;

        if (mip)
        {
            mipSize = static_cast<uint>(log2(maxDim));
            DirectX::GenerateMipMaps(*image, DirectX::TEX_FILTER_DEFAULT, mipSize, mipImage);
            image = mipImage.GetImages();
        }

        std::vector<D3D12_SUBRESOURCE_DATA> subresources;

        for (uint i = 0; i < mipSize; ++i)
        {
            D3D12_SUBRESOURCE_DATA subresource;
            subresource.RowPitch = image[i].rowPitch;
            subresource.SlicePitch = image[i].slicePitch;
            subresource.pData = image[i].pixels;

            subresources.push_back(subresource);
        }

        const uint uWidth = static_cast<uint>(image->width);
        const uint uHeight = static_cast<uint>(image->height);
        buffer* buf = e_globBufAllocator.alloc(nullptr, uWidth * uHeight, 1, buf::GBF_SRV, buf::RESOURCE_TEXTURE, image->format, uWidth, uHeight, static_cast<UINT16>(mipSize));

        {
            const HRESULT allocResetResult = render::getCmdQueue(render::QUEUE_COPY)->getAllocator()->Reset();
            TC_CONDITION(SUCCEEDED(allocResetResult), "Failed to reset copy allocator for texture upload");
            const HRESULT listResetResult = render::getCmdQueue(render::QUEUE_COPY)->getCmdList()->Reset(render::getCmdQueue(render::QUEUE_COPY)->getAllocator().Get(), nullptr);
            TC_CONDITION(SUCCEEDED(listResetResult), "Failed to reset copy command list for texture upload");
            //render::getCmdQueue(render::QUEUE_COPY)->getCmdList()->Close();
            //// The "before" state is not important. It will be resolved by the resource state tracker.
            //CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer->resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
            //render::getCmdQueue(render::QUEUE_COPY)->getCmdList()->ResourceBarrier(1, &barrier);
        
            const UINT64 requiredSize = GetRequiredIntermediateSize(buf->getResource(), 0, static_cast<uint>(subresources.size()));
            
            // Create a temporary (intermediate) resource for uploading the subresources
            Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
            
            CD3DX12_HEAP_PROPERTIES heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
            
            const HRESULT intermediateResult = e_globRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediateResource));
            TC_CONDITION(SUCCEEDED(intermediateResult), "Failed to create intermediate upload resource");
            
            UpdateSubresources(render::getCmdQueue(render::QUEUE_COPY)->getCmdList().Get(), buf->getResource(), intermediateResource.Get(), 0, 0, static_cast<uint>(subresources.size()), subresources.data());

            intermediates.push_back(intermediateResource);
        }

        render::getCmdQueue(render::QUEUE_COPY)->execute({ render::getCmdQueue(render::QUEUE_COPY)->getCmdList().Get() });

        render::getCmdQueue(render::QUEUE_COPY)->flush();
        intermediates.clear();

        return buf;
    }
}

namespace buf
{
    bool copyResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource, void* data, uint offset, uint size)
    {
        if (data == nullptr)
        {
            return true;
        }

        UINT8* pVertexDataBegin = nullptr;
        CD3DX12_RANGE readRange(0, 0);
        const HRESULT mapResult = resource->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
        if (FAILED(mapResult) || pVertexDataBegin == nullptr)
        {
            TC_LOG_ERROR(std::format("buf::copyResource: Map failed (hr 0x{:08X}); upload skipped", static_cast<unsigned long>(mapResult)).c_str());
            return false;
        }
        memcpy(pVertexDataBegin + offset, data, size);
        resource->Unmap(0, nullptr);
        return true;
    }

    D3D12_RESOURCE_STATES createResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource, CD3DX12_RESOURCE_DESC* bufDesc, uint size, void* data, CD3DX12_CLEAR_VALUE* clear, uint flags)
    {
        CD3DX12_HEAP_PROPERTIES heap_property;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

        if (flags & RESOURCE_UPLOAD)
        {
            heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            state = D3D12_RESOURCE_STATE_GENERIC_READ;
        }
        else if (flags & RESOURCE_READBACK)
        {
            heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
            state = D3D12_RESOURCE_STATE_COPY_DEST;
        }
        else if (flags & RESOURCE_COPY)
        {
            heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            state = D3D12_RESOURCE_STATE_COPY_DEST;
        }
        else
        {
            heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            state = D3D12_RESOURCE_STATE_COMMON;
        }

        if (flags & RESOURCE_DEPTH)
        {
            state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }

        const HRESULT committedResult = e_globRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, bufDesc,
            state, clear, IID_PPV_ARGS(&resource));
        TC_CONDITION(SUCCEEDED(committedResult), "Failed to create committed resource");

        if (data)
        {
            if (flags & RESOURCE_UPLOAD)
            {
                copyResource(resource, data, 0, size);
            }
            else
            {
                //cannot do direct map/unmap for non-upload resources
                e_globRenderer.uploadCopyGPUBuffer(resource.Get(), data, size);
            }
        }

        return state;
    }

	bool loadResources()
	{
        //create depth buffer
        //{
        //    uint width = e_globWindow.width();
        //    uint height = e_globWindow.height();

        //    buffer* depth = e_globBufAllocator.alloc(nullptr, 0, 3, GBF_DEPTH_STENCIL, buf::RESOURCE_DEPTH | buf::RESOURCE_TEXTURE, DXGI_FORMAT_D32_FLOAT, width, height, 1);
        //}

        //create image buffer
        {
            uint width = e_globWindow.width();
            uint height = e_globWindow.height();

            e_globBufAllocator.alloc(nullptr, 0, 3, buf::GBF_RT | buf::GBF_SRV, buf::RESOURCE_TEXTURE, DXGI_FORMAT_R32_UINT, width, height, 1);

            //{
            //    bufferContainer[IMAGE_IRRADIANCE] = loadTextureFromFile(L"asset/texture/Sierra_Madre_B_Ref.irr.hdr", true);
            //    bufferContainer[IMAGE_ENVIRONMENT] = loadTextureFromFile(L"asset/texture/Sierra_Madre_B_Ref.hdr", true);
            //}

            //after copying all image buffer flush
            //cmdqueue::getCmdQueue(cmdqueue::QUEUE_COPY)->execute({ render::getCmdQueue(render::QUEUE_COPY)->getCmdList().Get() });
            //
            //cmdqueue::getCmdQueue(cmdqueue::QUEUE_COPY)->flush();
            //intermediates.clear();
        }

		return true;
	}

    void cleanUp()
    {
    }


    //uint_8
    static_assert(graphicBufferFlags::GBF_COUNT < 8);

    constexpr uint DESCRIPTOR_SIZE = sizeof(descriptor);

    uint estimateBufferSize(uint_8 flags)
    {
        uint count = 0;

        for (uint i = 0; i < graphicBufferFlags::GBF_COUNT; ++i)
        {
            if(flags & (1 << i)) count += DESCRIPTOR_SIZE;
        }

        //buffer struct
        count += sizeof(buffer);

        return count;
    }

    char* viewAllocator::allocateUAVs(char* viewPos, buffer* buf, [[maybe_unused]] const CD3DX12_RESOURCE_DESC& bufDesc, UINT16 mipSlice, [[maybe_unused]] UINT16 mipViewCount)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC view = {};
        if (bufDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        {
            view.Format = bufDesc.Format;
            view.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            view.Texture2D.MipSlice = mipSlice;
            view.Texture2D.PlaneSlice = 0;
        }
        else
        {
            //we will forcely use R32 for UAVs since we will use our own packing unpacking for raw byte buffer
            view.Format = DXGI_FORMAT_R32_FLOAT;
            view.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            view.Buffer.FirstElement = 0;
            view.Buffer.NumElements = buf->header.dataSize / sizeof(float);
        }

        descriptor* descriptorPos = reinterpret_cast<descriptor*>(viewPos);

        *descriptorPos = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_UAV_TYPE, buf, &view);

#if ENGINE_DEBUG_RESOURCEVIEW
        recordResourceView(buf, *descriptorPos, buf::BUFFER_UAV_TYPE);
#endif // ENGINE_DEBUG_RESOURCEVIEW

        return viewPos + sizeof(descriptor);
    }

    char* viewAllocator::allocateCBVs(char* viewPos, buffer* buf, [[maybe_unused]] const CD3DX12_RESOURCE_DESC& bufDesc, [[maybe_unused]] UINT16 mipSlice, [[maybe_unused]] UINT16 mipViewCount)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC view = {};

        const uint size = buf->header.dataSize;

        view.BufferLocation = buf->resource->GetGPUVirtualAddress();
        view.SizeInBytes = size;

        descriptor* descriptorPos = reinterpret_cast<descriptor*>(viewPos);

        *descriptorPos = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_CONSTANT_TYPE, buf, &view);

#if ENGINE_DEBUG_RESOURCEVIEW
        recordResourceView(buf, *descriptorPos, buf::BUFFER_CONSTANT_TYPE);
#endif // ENGINE_DEBUG_RESOURCEVIEW

        return viewPos + sizeof(descriptor);
    }

    char* viewAllocator::allocateSRVs(char* viewPos, buffer* buf, const CD3DX12_RESOURCE_DESC& bufDesc, UINT16 mipSlice, UINT16 mipViewCount)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC view = {};
        descriptor* descriptorPos = reinterpret_cast<descriptor*>(viewPos);

        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if (buf->header.packedData.texture)
        {
            view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            view.Texture2D.MipLevels = (mipViewCount == 0) ? bufDesc.MipLevels : mipViewCount;
            view.Texture2D.MostDetailedMip = mipSlice;
            view.Format = bufDesc.Format;
        }
        else
        {
            D3D12_BUFFER_SRV desc = {};
            desc.NumElements = buf->header.dataSize / sizeof(float);
            desc.StructureByteStride = 0;
            desc.FirstElement = 0;
            desc.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

            view.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            view.Format = DXGI_FORMAT_R32_TYPELESS;
            view.Buffer = desc;
        }

        *descriptorPos = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, buf, &view);

#if ENGINE_DEBUG_RESOURCEVIEW
        recordResourceView(buf, *descriptorPos, buf::BUFFER_IMAGE_TYPE);
#endif // ENGINE_DEBUG_RESOURCEVIEW

        return viewPos + sizeof(descriptor);
    }

    char* viewAllocator::allocateDepthViews(char* viewPos, buffer* buf, [[maybe_unused]] const CD3DX12_RESOURCE_DESC& bufDesc, [[maybe_unused]] UINT16 mipSlice, [[maybe_unused]] UINT16 mipViewCount)
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC view = {};

        //TODO : pass the value for format size
        view.Format = DXGI_FORMAT_D32_FLOAT;
        view.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        view.Flags = D3D12_DSV_FLAG_NONE;

        descriptor* descriptorPos = reinterpret_cast<descriptor*>(viewPos);

        *descriptorPos = render::getHeap(render::DESCRIPTORHEAP_DEPTH)->requestdescriptor(buf::BUFFER_DEPTH_TYPE, buf, &view);

#if ENGINE_DEBUG_RESOURCEVIEW
        recordResourceView(buf, *descriptorPos, buf::BUFFER_DEPTH_TYPE);
#endif // ENGINE_DEBUG_RESOURCEVIEW

        return viewPos + sizeof(descriptor);
    }

    char* viewAllocator::allocateRenderTarget(char* viewPos, buffer* buf, const CD3DX12_RESOURCE_DESC& bufDesc, [[maybe_unused]] UINT16 mipSlice, [[maybe_unused]] UINT16 mipViewCount)
    {
        D3D12_RENDER_TARGET_VIEW_DESC view = {};
        view.Format = bufDesc.Format;
        //force render target to texture2d
        view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        descriptor* descriptorPos = reinterpret_cast<descriptor*>(viewPos);

        *descriptorPos = render::getHeap(render::DESCRIPTORHEAP_RENDERTARGET)->requestdescriptor(buf::BUFFER_RT_TYPE, buf, &view);

#if ENGINE_DEBUG_RESOURCEVIEW
        recordResourceView(buf, *descriptorPos, buf::BUFFER_RT_TYPE);
#endif // ENGINE_DEBUG_RESOURCEVIEW

        return viewPos + sizeof(descriptor);
    }
};

uint buffer::getElemSize() const
{
    return header.dataSize / (header.packedData.stride * sizeof(float));
}

uint buffer::getId() const
{
    return header.packedData.bufferId;
}

ID3D12Resource* buffer::getResource() const
{
    return resource.Get();
}

buffer_header* buffer::getHeader()
{
    return &header;
}

const buffer_header* buffer::getHeader() const
{
    return &header;
}

bool buffer::uploadBuffer(uint size, uint offset, void* data)
{
    if (data == nullptr)
    {
        TC_LOG_ERROR("buffer::uploadBuffer: data is nullptr; nothing uploaded");
        return false;
    }

    if (size == 0)
    {
        return true;
    }

    const UINT64 capacity = static_cast<UINT64>(header.dataSize);
    const UINT64 end = static_cast<UINT64>(offset) + static_cast<UINT64>(size);
    if (end > capacity)
    {
        TC_LOG_ERROR(std::format("buffer::uploadBuffer: offset {} + size {} exceeds capacity {} (buffer id {}); upload rejected",
            offset, size, header.dataSize, static_cast<uint>(header.packedData.bufferId)).c_str());
        return false;
    }

    UINT8* pVertexDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    const HRESULT mapResult = resource->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(mapResult) || pVertexDataBegin == nullptr)
    {
        TC_LOG_ERROR(std::format("buffer::uploadBuffer: Map failed (hr 0x{:08X}, buffer id {}); upload skipped",
            static_cast<unsigned long>(mapResult), static_cast<uint>(header.packedData.bufferId)).c_str());
        return false;
    }

    memcpy(pVertexDataBegin + offset, data, size);
    resource->Unmap(0, nullptr);
    return true;
}

descriptor* buffer::getDesc(buf::graphicBufferFlags flag)
{
    uint offset = 0;
    for (uint j = 1; j < flag; j <<= 1)
    {
        if (header.packedData.viewFlags & j)
        {
            offset += buf::DESCRIPTOR_SIZE;
        }
    }

    TC_ASSERT(header.packedData.viewFlags & flag);

    char* loc = reinterpret_cast<char*>(this) + offset + BUFFER_VIEW_OFFSET;

    return reinterpret_cast<descriptor*>(loc);
}

CD3DX12_RESOURCE_BARRIER buffer::getTransition(D3D12_RESOURCE_STATES after)
{
    if (curState == after) TC_LOG_WARNING("You are trying to transit same state");

    const D3D12_RESOURCE_STATES before = curState;
    curState = after;

    return CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), before, after);
}

D3D12_RESOURCE_STATES buffer::getCurResourceState() const
{
    return curState;
}

bool buffer::mapBuffer(unsigned char** dataPtr)
{
    if (dataPtr == nullptr)
    {
        TC_LOG_ERROR("buffer::mapBuffer: dataPtr is nullptr; not mapped");
        return false;
    }

    *dataPtr = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    const HRESULT mapResult = resource->Map(0, &readRange, reinterpret_cast<void**>(dataPtr));
    if (FAILED(mapResult) || *dataPtr == nullptr)
    {
        *dataPtr = nullptr;
        TC_LOG_ERROR(std::format("buffer::mapBuffer: Map failed (hr 0x{:08X}, buffer id {})",
            static_cast<unsigned long>(mapResult), static_cast<uint>(header.packedData.bufferId)).c_str());
        return false;
    }
    return true;
}

void buffer::unmapBuffer()
{
    resource->Unmap(0, nullptr);
}

bool buffer::mapReadbackBuffer(unsigned char** dataPtr, uint readSize)
{
    CD3DX12_RANGE readRange(0, readSize);
    const HRESULT hr = resource->Map(0, &readRange, reinterpret_cast<void**>(dataPtr));
    if (FAILED(hr) || *dataPtr == nullptr)
    {
        *dataPtr = nullptr;
        return false;
    }
    return true;
}

void buffer::unmapReadbackBuffer()
{
    CD3DX12_RANGE writtenRange(0, 0);
    resource->Unmap(0, &writtenRange);
}

void buffer_allocator::freeInternal(uint startPos, uint size, uint bufferId)
{
    const uint memSize = (uint)freedMem.size();

    for (uint i = 0; i + 1 < memSize; ++i)
    {
        auto& current = freedMem[i];
        bool leftMerged = false;

        if (startPos < current.first) continue;

        //check left
        if ((current.first + current.second) == startPos)
        {
            current.second += size;
            leftMerged = true;
        }

        //check right
        auto& next = freedMem[i + 1];
        if (next.first == (startPos + size))
        {
            if (leftMerged)
            {
                current.second += next.second;

                freedMem.erase(freedMem.begin() + i + 1);
            }
            else
            {
                next.first = startPos;
            }
        }
        else if (next.first > (startPos + size))
        {
            if (!leftMerged)
            {
                freedMem.insert(freedMem.begin() + i + 1, std::make_pair(startPos, size));
            }
        }
        else
        {
            TC_LOG_ERROR("freeInternal Error! something wrong with freedMem");
        }

        break;
    }


    for (auto iter = memBlocks.begin(); iter < memBlocks.end(); ++iter)
    {
        auto& current = *iter;

        if (current->header.packedData.bufferId == bufferId)
        {
#if ENGINE_DEBUG_RESOURCEVIEW
            if (bufferId < BUFFER_MAX_COUNT)
            {
                buf::debugInfoTable[bufferId].owner = nullptr;
            }
#endif // ENGINE_DEBUG_RESOURCEVIEW

            memBlocks.erase(iter);
            return;
        }
    }

    TC_LOG_ERROR("freeInternal Error! something wrong with memBlocks");
}

void buffer_allocator::init()
{
    data = new char[BUFFER_MAX_SIZE];
    nextID = 0;

    freedMem.push_back(std::make_pair(0, BUFFER_MAX_SIZE - 1));
}

buffer* buffer_allocator::alloc(char* bufferData, uint size, uint stride, uint_8 viewFlags,
    uint flag, DXGI_FORMAT format, UINT64 width, UINT height, UINT16 mipLevels, DirectX::XMFLOAT4 clearColor, ID3D12Resource* resource, UINT16 mipSlice, UINT16 mipViewCount, const char* debugName, std::source_location debugLoc)
{
    //cbv size should be multiplied by 256 bytes
    if (viewFlags & (buf::graphicBufferFlags::GBF_CBV))
    {
        size = ((size / CBVALLIGNMENT) + 1) * CBVALLIGNMENT;
    }

    uint totalSize = buf::estimateBufferSize(viewFlags);

    if(bufferData) totalSize += size;

    int pos = -1;

    bool found = false;

    for (auto iter = freedMem.begin(); iter < freedMem.end(); ++iter)
    {
        if ((*iter).second >= totalSize)
        {
            const uint remainSize = (*iter).second - totalSize;
            const int allocatedPos = (*iter).first + totalSize;

            pos = (*iter).first;

            (*iter).first = allocatedPos;
            (*iter).second = remainSize;

            found = true;

            break;
        }
    }

    if (!found) return nullptr;
    if (pos == -1) return nullptr;

    char* allocatedData = data + pos;

    buffer* buf = new (allocatedData) buffer();

    buf->header.dataSize = size;
    buf->header.totalSize = totalSize;
    buf->header.packedData.bufferId = nextID++;
    buf->header.packedData.allocated = 1;
    buf->header.packedData.viewFlags = viewFlags;
    buf->header.packedData.stride = stride;
    buf->header.packedData.lifetime = (flag & buf::RESOURCE_ONETIME) ? 1 : 0;
    buf->header.packedData.texture = (flag & buf::RESOURCE_TEXTURE) ? 1 : 0;
    buf->header.packedData.external = (resource != nullptr) ? 1 : 0;

    D3D12_RESOURCE_FLAGS resourceFlag = D3D12_RESOURCE_FLAG_NONE;

    if (viewFlags & (buf::graphicBufferFlags::GBF_DEPTH_STENCIL))
    {
        resourceFlag |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    if (viewFlags & (buf::graphicBufferFlags::GBF_UAV))
    {
        resourceFlag |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (viewFlags & (buf::graphicBufferFlags::GBF_RT))
    {
        resourceFlag |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (!(viewFlags & (buf::graphicBufferFlags::GBF_SRV)))
    {
        resourceFlag |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }

    CD3DX12_RESOURCE_DESC bufDesc;
    
    if (flag & buf::RESOURCE_TEXTURE)
    {
        bufDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, (UINT16)mipLevels, 1, 0);
    }
    else
    {
        bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
    }

    bufDesc.Flags = resourceFlag;

    CD3DX12_CLEAR_VALUE clearValue;

    if (resource == nullptr)
    {
        if (flag & buf::RESOURCE_CLEAR)
        {
            if ((flag & buf::RESOURCE_DEPTH))
            {
                clearValue = CD3DX12_CLEAR_VALUE(format, 0.0f, 0);
            }
            else if (flag & buf::RESOURCE_TEXTURE)
            {
                float color[4];
                color[0] = clearColor.x;
                color[1] = clearColor.y;
                color[2] = clearColor.z;
                color[3] = clearColor.w;

                clearValue = CD3DX12_CLEAR_VALUE(format, color);
            }

            buf->curState = buf::createResource(buf->resource, &bufDesc, size, bufferData, &clearValue, flag);
        }
        else
        {
            buf->curState = buf::createResource(buf->resource, &bufDesc, size, bufferData, nullptr, flag);
        }
    }
    else
    {
        if (flag & buf::RESOURCE_SHARED)
        {
            buf->resource = resource;
        }
        else
        {
            buf->resource.Attach(resource);
        }
        buf->curState = D3D12_RESOURCE_STATE_COMMON;
    }

#if ENGINE_DEBUG_RESOURCEVIEW
    {
        const uint bufferId = buf->header.packedData.bufferId;
        if (bufferId < BUFFER_MAX_COUNT)
        {
            buf::debugInfoTable[bufferId].views.clear();
            buf::debugInfoCount = std::max(buf::debugInfoCount, bufferId + 1);
        }
    }
#endif // ENGINE_DEBUG_RESOURCEVIEW

    //views
    char* viewPos = allocatedData + BUFFER_VIEW_OFFSET;

    for (uint i = 0; i < buf::graphicBufferFlags::GBF_COUNT; ++i)
    {
        if (viewFlags & (1 << i))
        {
            viewPos = buf::allocateViews[i](viewPos, buf, bufDesc, mipSlice, mipViewCount);
        }
    }

    buf->baseLoc = viewPos;

    //cpu data
    if (bufferData)
    {
        memcpy(viewPos, bufferData, size);
        viewPos += size;
    }

    const uint addrDiff = (uint)(viewPos - allocatedData);

    TC_ASSERT(addrDiff == totalSize);

#if ENGINE_DEBUG_RESOURCEVIEW
    {
        const uint bufferId = buf->header.packedData.bufferId;
        if (bufferId < BUFFER_MAX_COUNT)
        {
            buf::resourceDebugInfo& info = buf::debugInfoTable[bufferId];
            info.valid = true;
            info.owner = buf;
            info.cpuArenaBytes = buf->header.totalSize;
            info.viewFlags = buf->header.packedData.viewFlags;
            info.texture = buf->header.packedData.texture ? true : false;
            info.external = buf->header.packedData.external ? true : false;
            info.oneTime = buf->header.packedData.lifetime ? true : false;

            // Format the name once into info.name
#if ENGINE_DEBUG_RESOURCENAME
            if (debugName != nullptr)
            {
                snprintf(info.name, sizeof(info.name), "%s", debugName);
            }
            else
            {
                const char* fileName = debugLoc.file_name();
                const char* lastSlash = strrchr(fileName, '\\');
                if (lastSlash != nullptr)
                {
                    fileName = lastSlash + 1;
                }
                snprintf(info.name, sizeof(info.name), "%s:%u", fileName, debugLoc.line());
            }
#else // ENGINE_DEBUG_RESOURCENAME
            {
                const char* fileName = debugLoc.file_name();
                const char* lastSlash = strrchr(fileName, '\\');
                if (lastSlash != nullptr)
                {
                    fileName = lastSlash + 1;
                }
                snprintf(info.name, sizeof(info.name), "%s:%u", fileName, debugLoc.line());
            }
            (void)debugName;
#endif // ENGINE_DEBUG_RESOURCENAME

            const char* fileName = debugLoc.file_name();
            const char* lastSlash = strrchr(fileName, '\\');
            if (lastSlash != nullptr)
            {
                fileName = lastSlash + 1;
            }
            info.file = fileName;
            info.line = debugLoc.line();

            // Cache immutable resource metadata
            ID3D12Resource* res = buf->resource.Get();
            if (res != nullptr)
            {
                const D3D12_RESOURCE_DESC desc = res->GetDesc();
                info.dimension = desc.Dimension;
                info.format = desc.Format;
                info.width = desc.Width;
                info.height = desc.Height;
                info.mipLevels = desc.MipLevels;

                info.gpuBytes = e_globRenderer.device->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes;

                D3D12_HEAP_PROPERTIES props = {};
                D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
                res->GetHeapProperties(&props, &heapFlags);
                info.heapType = props.Type;
            }
            else
            {
                info.gpuBytes = 0;
                info.heapType = D3D12_HEAP_TYPE_DEFAULT;
                info.dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN;
                info.format = DXGI_FORMAT_UNKNOWN;
                info.width = 0;
                info.height = 0;
                info.mipLevels = 0;
            }
        }
    }
#endif // ENGINE_DEBUG_RESOURCEVIEW

    memBlocks.push_back(buf);

    return buf;
}

void buffer_allocator::free(char* bufferData)
{
    const buffer* buf = reinterpret_cast<buffer*>(bufferData);

    freeInternal(static_cast<uint>(data - bufferData), buf->header.totalSize, buf->header.packedData.bufferId);
}

void buffer_allocator::free(uint index)
{
    bool found = false;
    std::ptrdiff_t startDiff = 0;
    uint size = 0;

    for (auto& mem : memBlocks)
    {
        if (mem->header.packedData.bufferId == index)
        {
            startDiff = reinterpret_cast<char*>(mem) - data;
            found = true;
            size = mem->header.totalSize;
        }
    }

    if (!found)
    {
        TC_LOG_ERROR("Trying to free vacant buffer");
        return;
    }

    if (startDiff < 0 || startDiff > static_cast<std::ptrdiff_t>((std::numeric_limits<uint>::max)()))
    {
        TC_LOG_ERROR("Trying to free buffer with an out-of-range arena offset");
        return;
    }

    freeInternal(static_cast<uint>(startDiff), size, index);
}

#if ENGINE_DEBUG_RESOURCEVIEW
uint buffer_allocator::getArenaCapacity() const
{
    return BUFFER_MAX_SIZE;
}

uint buffer_allocator::getArenaUsed() const
{
    uint used = 0;
    for (const auto& freeBlock : freedMem)
    {
        used += freeBlock.second;
    }
    return BUFFER_MAX_SIZE - used;
}

uint buffer_allocator::getFreeBlockCount() const
{
    return (uint)freedMem.size();
}

void buffer_allocator::getFreeBlock(uint i, uint& start, uint& size) const
{
    if (i < freedMem.size())
    {
        start = freedMem[i].first;
        size = freedMem[i].second;
    }
    else
    {
        start = 0;
        size = 0;
    }
}

namespace buf
{
    inline const char* formatBytes(UINT64 bytes)
    {
        static char buffer[32];
        if (bytes < 1024)
        {
            snprintf(buffer, sizeof(buffer), "%llu B", bytes);
        }
        else if (bytes < 1024 * 1024)
        {
            snprintf(buffer, sizeof(buffer), "%.2f KiB", bytes / 1024.0);
        }
        else if (bytes < 1024 * 1024 * 1024)
        {
            snprintf(buffer, sizeof(buffer), "%.2f MiB", bytes / (1024.0 * 1024.0));
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "%.2f GiB", bytes / (1024.0 * 1024.0 * 1024.0));
        }
        return buffer;
    }

    static const char* viewTypeLabel(BUFFER_TYPE type)
    {
        switch (type)
        {
        case BUFFER_VERTEX_TYPE:
            return "VB";
        case BUFFER_CONSTANT_TYPE:
            return "CBV";
        case BUFFER_UAV_TYPE:
            return "UAV";
        case BUFFER_IMAGE_TYPE:
            return "SRV";
        case BUFFER_RT_TYPE:
            return "RTV";
        case BUFFER_INDEX_TYPE:
            return "IB";
        case BUFFER_DEPTH_TYPE:
            return "DSV";
        default:
            return "UNKNOWN";
        }
    }

    void guiResourceViewerSetting()
    {
        uint liveCount = 0;
        uint viewCount = 0;
        uint viewsPerHeap[render::DESCRIPTORHEAP_MAX] = {};
        for (uint id = 0; id < debugInfoCount; ++id)
        {
            const resourceDebugInfo& info = debugInfoTable[id];
            if (info.valid && info.owner != nullptr)
            {
                liveCount++;
                viewCount += (uint)info.views.size();
                for (const auto& rec : info.views)
                {
                    if (rec.heapIndex < render::DESCRIPTORHEAP_MAX)
                    {
                        viewsPerHeap[rec.heapIndex]++;
                    }
                }
            }
        }

        ImGui::Text("%u views across %u resources | RTV %u | CBV_SRV_UAV %u | DSV %u",
                    viewCount, liveCount, viewsPerHeap[render::DESCRIPTORHEAP_RENDERTARGET],
                    viewsPerHeap[render::DESCRIPTORHEAP_BUFFER], viewsPerHeap[render::DESCRIPTORHEAP_DEPTH]);

        const ImVec2 tableOuterSize(0.0f, ImGui::GetContentRegionAvail().y * 0.65f);
        if (ImGui::BeginTable("ViewList", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, tableOuterSize))
        {
            ImGui::TableSetupColumn("Resource");
            ImGui::TableSetupColumn("Id");
            ImGui::TableSetupColumn("View Type");
            ImGui::TableSetupColumn("Desc Heap");
            ImGui::TableSetupColumn("Slot");
            ImGui::TableSetupColumn("Kind");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("State");
            ImGui::TableHeadersRow();

            uint rowIndex = 0;
            for (uint id = 0; id < debugInfoCount; ++id)
            {
                const resourceDebugInfo& info = debugInfoTable[id];
                if (!info.valid || info.owner == nullptr || info.views.empty())
                {
                    continue;
                }

                for (uint v = 0; v < info.views.size(); ++v)
                {
                    const auto& rec = info.views[v];

                    ImGui::PushID((int)rowIndex);
                    ImGui::TableNextRow();

                    if (info.external)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                    }

                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable(info.name, render::getSelectedResourceId() == id, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        render::setSelectedResourceId(render::getSelectedResourceId() == id ? ~0u : id);
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%u", id);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s", viewTypeLabel(rec.viewType));

                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%s", render::getDescHeapLabel(static_cast<render::descriptorHeapIndex>(rec.heapIndex)));

                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%u", rec.heapOffset);

                    ImGui::TableSetColumnIndex(5);
                    if (info.texture)
                    {
                        const char* dimStr = "UNKNOWN";
                        switch (info.dimension)
                        {
                        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
                            dimStr = "TEXTURE2D";
                            break;
                        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
                            dimStr = "TEXTURE3D";
                            break;
                        default:
                            break;
                        }
                        ImGui::Text("%s / %u", dimStr, info.format);
                    }
                    else
                    {
                        ImGui::Text("BUFFER");
                    }

                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%s", formatBytes(info.gpuBytes));

                    ImGui::TableSetColumnIndex(7);
                    const char* stateStr = "COMMON";
                    if (info.owner != nullptr)
                    {
                        const D3D12_RESOURCE_STATES state = info.owner->getCurResourceState();
                        switch (state)
                        {
                        case D3D12_RESOURCE_STATE_GENERIC_READ:
                            stateStr = "READ";
                            break;
                        case D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
                            stateStr = "UAV";
                            break;
                        case D3D12_RESOURCE_STATE_COPY_DEST:
                            stateStr = "COPY_DST";
                            break;
                        case D3D12_RESOURCE_STATE_COPY_SOURCE:
                            stateStr = "COPY_SRC";
                            break;
                        default:
                            break;
                        }
                    }
                    ImGui::Text("%s", stateStr);

                    if (info.external)
                    {
                        ImGui::PopStyleColor();
                    }

                    ImGui::PopID();
                    rowIndex++;
                }
            }

            ImGui::EndTable();
        }

        const uint selectedId = render::getSelectedResourceId();
        if (selectedId != ~0u)
        {
            ImGui::Separator();
            ImGui::Text("Selected: %s", getResourceDisplayName(selectedId));

            if (selectedId < debugInfoCount)
            {
                const resourceDebugInfo& info = debugInfoTable[selectedId];
                if (info.valid && info.owner != nullptr)
                {
                    ImGui::Text("BufferId: %u", selectedId);
                    ImGui::Text("CPU Arena: %u bytes", info.cpuArenaBytes);
                    ImGui::Text("GPU Memory: %s", formatBytes(info.gpuBytes));
                    ImGui::Text("Dim/Format/Mips: %u %u %u", info.width, info.height, info.mipLevels);
                    ImGui::Text("Views: %u", (uint)info.views.size());
                }
            }
        }
    }

    void guiMemoryViewerSetting()
    {
        const uint capacity = e_globBufAllocator.getArenaCapacity();
        const uint used = e_globBufAllocator.getArenaUsed();
        const float fraction = (float)used / (float)capacity;

        ImGui::ProgressBar(fraction, ImVec2(-1, 0), "");
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::Text("%u / %u MiB", used / (1024 * 1024), capacity / (1024 * 1024));

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 contentPos = ImGui::GetCursorScreenPos();
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        contentSize.y = 30;

        const float barWidth = contentSize.x;
        const ImU32 allocColor = ImGui::GetColorU32(ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        const ImU32 freeColor = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

        uint allocatedStart = 0;
        const uint freeBlockCount = e_globBufAllocator.getFreeBlockCount();
        for (uint i = 0; i < freeBlockCount; ++i)
        {
            uint start = 0, size = 0;
            e_globBufAllocator.getFreeBlock(i, start, size);

            if (start > allocatedStart)
            {
                float startFrac = (float)allocatedStart / (float)capacity;
                float sizeFrac = (float)(start - allocatedStart) / (float)capacity;
                float x0 = contentPos.x + startFrac * barWidth;
                float x1 = x0 + sizeFrac * barWidth;
                drawList->AddRectFilled(ImVec2(x0, contentPos.y), ImVec2(x1, contentPos.y + contentSize.y), allocColor);
            }

            float startFrac = (float)start / (float)capacity;
            float sizeFrac = (float)size / (float)capacity;
            float x0 = contentPos.x + startFrac * barWidth;
            float x1 = x0 + sizeFrac * barWidth;
            drawList->AddRectFilled(ImVec2(x0, contentPos.y), ImVec2(x1, contentPos.y + contentSize.y), freeColor);

            allocatedStart = start + size;
        }

        if (allocatedStart < capacity)
        {
            float startFrac = (float)allocatedStart / (float)capacity;
            float sizeFrac = (float)(capacity - allocatedStart) / (float)capacity;
            float x0 = contentPos.x + startFrac * barWidth;
            float x1 = x0 + sizeFrac * barWidth;
            drawList->AddRectFilled(ImVec2(x0, contentPos.y), ImVec2(x1, contentPos.y + contentSize.y), allocColor);
        }

        ImGui::Dummy(ImVec2(0, 30));

        uint debugLiveBufferCount = 0;
        for (uint id = 0; id < debugInfoCount; ++id)
        {
            if (debugInfoTable[id].valid && debugInfoTable[id].owner != nullptr)
            {
                debugLiveBufferCount++;
            }
        }

        ImGui::Text("Live buffers: %u", debugLiveBufferCount);
        ImGui::Text("Free blocks: %u", freeBlockCount);

        ImGui::Separator();

        UINT64 localBudget = 0, localUsage = 0;
        UINT64 nonLocalBudget = 0, nonLocalUsage = 0;
        const bool localAvailable = e_globRenderer.getVideoMemoryInfo(DXGI_MEMORY_SEGMENT_GROUP_LOCAL, localBudget, localUsage);
        const bool nonLocalAvailable = e_globRenderer.getVideoMemoryInfo(DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, nonLocalBudget, nonLocalUsage);

        if (localAvailable)
        {
            char localUsageText[32];
            char localBudgetText[32];
            snprintf(localUsageText, sizeof(localUsageText), "%s", formatBytes(localUsage));
            snprintf(localBudgetText, sizeof(localBudgetText), "%s", formatBytes(localBudget));
            ImGui::Text("Local VRAM: %s / %s", localUsageText, localBudgetText);
        }
        else
        {
            ImGui::Text("Local VRAM: unavailable");
        }

        if (nonLocalAvailable)
        {
            char nonLocalUsageText[32];
            char nonLocalBudgetText[32];
            snprintf(nonLocalUsageText, sizeof(nonLocalUsageText), "%s", formatBytes(nonLocalUsage));
            snprintf(nonLocalBudgetText, sizeof(nonLocalBudgetText), "%s", formatBytes(nonLocalBudget));
            ImGui::Text("Non-Local VRAM: %s / %s", nonLocalUsageText, nonLocalBudgetText);
        }
        else
        {
            ImGui::Text("Non-Local VRAM: unavailable");
        }

        ImGui::Separator();

        struct HeapGroup
        {
            D3D12_HEAP_TYPE type;
            const char* label;
            UINT64 totalSize;
            uint resourceCount;
            uint maxResourceSize;
        };

        HeapGroup heapGroups[5] = {
            { D3D12_HEAP_TYPE_DEFAULT, "Default", 0, 0, 0 },
            { D3D12_HEAP_TYPE_UPLOAD, "Upload", 0, 0, 0 },
            { D3D12_HEAP_TYPE_READBACK, "Readback", 0, 0, 0 },
            { D3D12_HEAP_TYPE_CUSTOM, "Custom", 0, 0, 0 },
            { (D3D12_HEAP_TYPE)999, "Attached (External)", 0, 0, 0 }
        };

        for (uint id = 0; id < debugInfoCount; ++id)
        {
            const resourceDebugInfo& info = debugInfoTable[id];
            if (!info.valid || info.owner == nullptr)
            {
                continue;
            }

            int groupIdx = -1;
            if (info.external)
            {
                groupIdx = 4;
            }
            else if (info.heapType == D3D12_HEAP_TYPE_DEFAULT)
            {
                groupIdx = 0;
            }
            else if (info.heapType == D3D12_HEAP_TYPE_UPLOAD)
            {
                groupIdx = 1;
            }
            else if (info.heapType == D3D12_HEAP_TYPE_READBACK)
            {
                groupIdx = 2;
            }
            else if (info.heapType == D3D12_HEAP_TYPE_CUSTOM)
            {
                groupIdx = 3;
            }

            if (groupIdx >= 0)
            {
                if (!info.external)
                {
                    heapGroups[groupIdx].totalSize += info.gpuBytes;
                }
                heapGroups[groupIdx].resourceCount++;
                if (info.gpuBytes > heapGroups[groupIdx].maxResourceSize)
                {
                    heapGroups[groupIdx].maxResourceSize = (uint)info.gpuBytes;
                }
            }
        }

        for (int i = 0; i < 5; ++i)
        {
            if (heapGroups[i].resourceCount > 0)
            {
                char headerLabel[256];
                char totalSizeText[32];
                snprintf(totalSizeText, sizeof(totalSizeText), "%s", formatBytes(heapGroups[i].totalSize));
                snprintf(headerLabel, sizeof(headerLabel), "%s - %u resources, %s", heapGroups[i].label, heapGroups[i].resourceCount, totalSizeText);
                if (ImGui::CollapsingHeader(headerLabel))
                {
                    for (uint id = 0; id < debugInfoCount; ++id)
                    {
                        const resourceDebugInfo& info = debugInfoTable[id];
                        if (!info.valid || info.owner == nullptr)
                        {
                            continue;
                        }

                        bool match = false;
                        if (i == 4 && info.external)
                        {
                            match = true;
                        }
                        else if (i < 4 && !info.external)
                        {
                            if ((i == 0 && info.heapType == D3D12_HEAP_TYPE_DEFAULT) ||
                                (i == 1 && info.heapType == D3D12_HEAP_TYPE_UPLOAD) ||
                                (i == 2 && info.heapType == D3D12_HEAP_TYPE_READBACK) ||
                                (i == 3 && info.heapType == D3D12_HEAP_TYPE_CUSTOM))
                            {
                                match = true;
                            }
                        }

                        if (match && heapGroups[i].maxResourceSize > 0)
                        {
                            const float barFrac = (float)info.gpuBytes / (float)heapGroups[i].maxResourceSize;
                            ImGui::ProgressBar(barFrac, ImVec2(-1, 0), "");
                            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                            ImGui::Text("%s (%s)", info.name, formatBytes(info.gpuBytes));
                        }
                    }
                }
            }
        }
    }
}
#endif // ENGINE_DEBUG_RESOURCEVIEW
