#include <rapidobj/rapidobj.hpp>

#include <render/buffer.hpp>
#include <render/commandqueue.hpp>
#include <render/renderer.hpp>
#include <render/mesh.hpp>
#include <system/window.hpp>
#include <system/logger.hpp>
#include <system/jsonhelper.hpp>

#include <vector>

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXTex.h>

constexpr uint CBVALLIGNMENT = 256;

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
    //will be replaced in future
    std::array<buffer*, DEPTH_END> bufferContainer;

    std::vector<buffer*> trackedBuffer;
 
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediates;

    void loadMeshInfo(std::string fileName, meshData* meshData)
    {
        auto pos = fileName.find(".obj");

        fileName = fileName.substr(0, pos) + ".info";

        char* data = nullptr;
        int size = rawFileRead(fileName, &data);

        TC_ASSERT(size >= 0);

        if (size != -1)
        {
            std::string stringData = data;
            auto pos = stringData.find("Number of LODs : ") + 16;

            std::string subStr = stringData.substr(pos);
            std::string lodNumString = subStr.substr(0, subStr.find('\n'));
            meshData->lodNum = std::stoi(lodNumString);

            subStr = subStr.substr(subStr.find("cluster Infos by LOD : \n") + 24);

            meshData->lodData.resize(meshData->lodNum);

            uint totalClusters = 0;
            for (uint i = 0; i < meshData->lodNum; ++i)
            {
                uint nextPos = subStr.find(" : ");
                std::string valueString = subStr.substr(0, nextPos);
                TC_ASSERT(std::stoi(valueString) == i);
                subStr = subStr.substr(nextPos + 3);
                nextPos = subStr.find(", ");
                valueString = subStr.substr(0, nextPos);
                meshData->lodData[i].clusterNum = std::stoi(valueString);
                totalClusters += meshData->lodData[i].clusterNum;
                subStr = subStr.substr(nextPos + 2);
                nextPos = subStr.find('\n');
                valueString = subStr.substr(0, nextPos);
                meshData->lodData[i].totalIndicesCount = std::stoi(valueString);

                nextPos = subStr.find('\n');
                subStr = subStr.substr(nextPos + 1);
            }

            uint nextPos = subStr.find("\n") + 1;
            subStr = subStr.substr(nextPos);

            for (uint i = 0; i < meshData->lodNum; ++i)
            {
                uint totalIndexCount = 0;

                for (uint j = 0; j < meshData->lodData[i].clusterNum; ++j)
                {
                    nextPos = subStr.find(", \n");
                    std::string valueString = subStr.substr(0, nextPos);
                    uint indexCount = std::stoi(valueString);
                    meshData->lodData[i].indexSize.push_back(indexCount);
                    totalIndexCount += indexCount;
                    
                    subStr = subStr.substr(nextPos + 1);
                }

                TC_ASSERT(totalIndexCount == meshData->lodData[i].totalIndicesCount);
            }
        }

        delete[] data;
    }

    void loadFiletoMesh(std::string fileName, meshData* meshdata)
    {
        auto result = rapidobj::ParseFile(fileName);

        if (result.error)
        {
            auto errorMsg = result.error.code.message();
            std::string errorLog = "Failed to load file : " + fileName + '\n' + errorMsg;
            TC_LOG_ERROR(errorLog.c_str());
            return;
        }

        rapidobj::Triangulate(result);

        if (result.error)
        {
            auto errorMsg = result.error.code.message();
            std::string errorLog = "Failed to triangulate file : " + fileName + '\n' + errorMsg;
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

#if ENGINE_DEBUG_DATATEST
        float xMin = FLT_MAX;
        float yMin = FLT_MAX;
        float zMin = FLT_MAX;
#endif // #if ENGINE_DEBUG_DATATEST

        for (uint i = 0; i < result.attributes.positions.size(); i += 3)
        {
            float x = result.attributes.positions[i + 0];
            float y = result.attributes.positions[i + 1];
            float z = result.attributes.positions[i + 2];

            meshdata->boundData.halfExtent[msh::AXIS_X] = std::max(meshdata->boundData.halfExtent[msh::AXIS_X], std::abs(x));
            meshdata->boundData.halfExtent[msh::AXIS_Y] = std::max(meshdata->boundData.halfExtent[msh::AXIS_Y], std::abs(y));
            meshdata->boundData.halfExtent[msh::AXIS_Z] = std::max(meshdata->boundData.halfExtent[msh::AXIS_Z], std::abs(z));

#if ENGINE_DEBUG_DATATEST
            xMin = std::min(xMin, x);
            yMin = std::min(yMin, y);
            zMin = std::min(zMin, z);
        }

        assert((xMin + meshdata->boundData.halfExtent[msh::AXIS_X]) < FLT_EPSILON);
        assert((yMin + meshdata->boundData.halfExtent[msh::AXIS_Y]) < FLT_EPSILON);
        assert((zMin + meshdata->boundData.halfExtent[msh::AXIS_Z]) < FLT_EPSILON);
#else // #if ENGINE_DEBUG_DATATEST
        }   
#endif // #else // #if ENGINE_DEBUG_DATATEST

        meshdata->vbs = createVertexBuffer(result.attributes.positions.data(), static_cast<uint>(sizeof(float) * result.attributes.positions.size()), sizeof(float) * 3);
        //make sure that the model file contains normal data and it's vertex normal not face normal
        if (result.attributes.normals.size() > 0)
        {
            meshdata->norm = createVertexBuffer(result.attributes.normals.data(), static_cast<uint>(sizeof(float) * result.attributes.normals.size()), sizeof(float) * 3);
            TC_ASSERT(result.attributes.normals.size() == result.attributes.positions.size());
        }
        else
        {
            meshdata->norm = nullptr;
        }
        meshdata->idx = createIndexBuffer(indices.data(), static_cast<uint>(sizeof(uint) * indices.size()));

        //TODO : not support now
        //meshdata->idxLine = createIndexBuffer(result.shapes[0].lines.indices.data(), static_cast<uint>(sizeof(uint) * result.shapes[0].lines.indices.size()));

        loadMeshInfo(fileName, meshdata);
    }

    imagebuffer* loadTextureFromFile(std::wstring filename, bool mip)
    {
        DirectX::TexMetadata metadata;
        DirectX::ScratchImage scratchImage;
        DirectX::ScratchImage mipImage;

        DirectX::LoadFromHDRFile(filename.c_str(), &metadata, scratchImage);

        //we assume there will be only one image from one file
        const DirectX::Image* image = scratchImage.GetImages();

        uint maxDim = std::max(static_cast<uint>(image->width), static_cast<uint>(image->height));

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

        imagebuffer* buffer = createImageBuffer(static_cast<uint>(image->width), static_cast<uint>(image->height), mipSize, image->format, D3D12_RESOURCE_FLAG_NONE);

        {
            render::getCmdQueue(render::QUEUE_COPY)->getAllocator()->Reset();
            render::getCmdQueue(render::QUEUE_COPY)->getCmdList()->Reset(render::getCmdQueue(render::QUEUE_COPY)->getAllocator().Get(), nullptr);
            //render::getCmdQueue(render::QUEUE_COPY)->getCmdList()->Close();
            //// The "before" state is not important. It will be resolved by the resource state tracker.
            //CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer->resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
            //render::getCmdQueue(render::QUEUE_COPY)->getCmdList()->ResourceBarrier(1, &barrier);
        
            UINT64 requiredSize = GetRequiredIntermediateSize(buffer->resource.Get(), 0, static_cast<uint>(subresources.size()));
            
            // Create a temporary (intermediate) resource for uploading the subresources
            Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
            
            CD3DX12_HEAP_PROPERTIES heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
            
            e_globRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediateResource));
            
            UpdateSubresources(render::getCmdQueue(render::QUEUE_COPY)->getCmdList().Get(), buffer->resource.Get(), intermediateResource.Get(), 0, 0, static_cast<uint>(subresources.size()), subresources.data());

            intermediates.push_back(intermediateResource);
        }

        render::getCmdQueue(render::QUEUE_COPY)->execute({ render::getCmdQueue(render::QUEUE_COPY)->getCmdList().Get() });

        render::getCmdQueue(render::QUEUE_COPY)->flush();
        intermediates.clear();

        return buffer;
    }
}

namespace buf
{
    void copyResource(buffer* buf, void* data, uint offset, uint size)
    {
        if (data != nullptr)
        {
            UINT8* pVertexDataBegin;
            CD3DX12_RANGE readRange(0, 0);
            buf->resource->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
            memcpy(pVertexDataBegin, data, size);
            buf->resource->Unmap(0, nullptr);
        }
    }

    bool createResource(buffer* buf, uint size, void* data)
    {
        CD3DX12_HEAP_PROPERTIES heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

        e_globRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf->resource));

        copyResource(buf, data, 0, size);

        return true;
    }

    bool createResource(buffer* buf, uint size, D3D12_RESOURCE_FLAGS flags)
    {
        CD3DX12_HEAP_PROPERTIES heap_property;
        D3D12_RESOURCE_STATES state;

        if (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
        {
            heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            state = D3D12_RESOURCE_STATE_COMMON;
        }
        else
        {
            heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            state = D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        bufDesc.Flags |= flags;

        e_globRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc,
            state, nullptr, IID_PPV_ARGS(&buf->resource));

        return true;
    }

    bool createNonTexture(buffer* buf, uint size, D3D12_RESOURCE_FLAGS flags)
    {
        CD3DX12_HEAP_PROPERTIES heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        bufDesc.Flags |= flags;
        D3D12_RESOURCE_STATES resourceStates = D3D12_RESOURCE_STATE_COMMON;

        //TODO depth_write will be changed
        e_globRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc,
            resourceStates, nullptr, IID_PPV_ARGS(&buf->resource));

        imagebuffer* imgBuffer = dynamic_cast<imagebuffer*>(buf);
        if (imgBuffer != nullptr)
        {
            imgBuffer->view.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            imgBuffer->view.Buffer.FirstElement = 0;
            imgBuffer->view.Buffer.NumElements = size / 4;
            imgBuffer->view.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        }

        return true;
    }

    bool createTexture(buffer* buf, DXGI_FORMAT format, uint width, uint height, unsigned int mipLevel, D3D12_RESOURCE_FLAGS flags, CD3DX12_CLEAR_VALUE* clear, bool depth = false)
    {
        CD3DX12_HEAP_PROPERTIES heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, (UINT16)mipLevel, 1, 0, flags);
        D3D12_RESOURCE_STATES resourceStates = D3D12_RESOURCE_STATE_COMMON;
        if (depth) resourceStates = D3D12_RESOURCE_STATE_DEPTH_WRITE;

        //TODO depth_write will be changed
        e_globRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc,
            resourceStates, clear, IID_PPV_ARGS(&buf->resource));

        imagebuffer* imgBuffer = dynamic_cast<imagebuffer*>(buf);
        if (imgBuffer != nullptr)
        {
            imgBuffer->view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            imgBuffer->view.Texture2D.MipLevels = mipLevel;
        }

        return true;
    }

	bool loadResources()
	{
        ////create vertex buffer
        //{
        //    //create triangle vertex
        //    {
        //        float triangleVertices[] =
        //        {
        //            0.0, 0.5, 0.0f,
        //            0.5, -0.5, 0.0f,
        //            0.0, -0.5, 0.0f,
        //        };

        //        bufferContainer[VERTEX_TRIANGLE] = createVertexBuffer(triangleVertices, sizeof(triangleVertices), sizeof(float) * 3);
        //    }

            //create triangle vertex
            {
                float cubeVertices[] =
                {
                    0.5f,  0.5f,  0.5f,
                    0.5f, -0.5f,  0.5f,
                   -0.5f,  0.5f,  0.5f,
                   -0.5f, -0.5f,  0.5f,

                    0.5f,  0.5f, -0.5f,
                    0.5f, -0.5f, -0.5f,
                   -0.5f,  0.5f, -0.5f,
                   -0.5f, -0.5f, -0.5f,

                    0.5f,  0.5f,  0.5f,
                    0.5f,  0.5f, -0.5f,
                   -0.5f,  0.5f,  0.5f,
                   -0.5f,  0.5f, -0.5f,

                    0.5f, -0.5f,  0.5f,
                    0.5f, -0.5f, -0.5f,
                   -0.5f, -0.5f,  0.5f,
                   -0.5f, -0.5f, -0.5f,

                    0.5f,  0.5f,  0.5f,
                    0.5f,  0.5f, -0.5f,
                    0.5f, -0.5f,  0.5f,
                    0.5f, -0.5f, -0.5f,

                   -0.5f,  0.5f,  0.5f,
                   -0.5f, -0.5f,  0.5f,
                   -0.5f,  0.5f, -0.5f,
                   -0.5f, -0.5f, -0.5f,
                };

                float cubeNorm[] = {
                    0.0f, 0.0f, 1.0f,
                    0.0f, 0.0f, 1.0f,
                    0.0f, 0.0f, 1.0f,
                    0.0f, 0.0f, 1.0f,

                    0.0f, 0.0f, -1.0f,
                    0.0f, 0.0f, -1.0f,
                    0.0f, 0.0f, -1.0f,
                    0.0f, 0.0f, -1.0f,

                    0.0f, 1.0f, 0.0f,
                    0.0f, 1.0f, 0.0f,
                    0.0f, 1.0f, 0.0f,
                    0.0f, 1.0f, 0.0f,

                    0.0f, -1.0f, 0.0f,
                    0.0f, -1.0f, 0.0f,
                    0.0f, -1.0f, 0.0f,
                    0.0f, -1.0f, 0.0f,

                    1.0f, 0.0f, 0.0f,
                    1.0f, 0.0f, 0.0f,
                    1.0f, 0.0f, 0.0f,
                    1.0f, 0.0f, 0.0f,

                    -1.0f, 0.0f, 0.0f,
                    -1.0f, 0.0f, 0.0f,
                    -1.0f, 0.0f, 0.0f,
                    -1.0f, 0.0f, 0.0f,
                };

                bufferContainer[VERTEX_CUBE] = createVertexBuffer(cubeVertices, sizeof(cubeVertices), sizeof(float) * 3);
                bufferContainer[VERTEX_CUBE_NORM] = createVertexBuffer(cubeNorm, sizeof(cubeNorm), sizeof(float) * 3);

                uint cubeIndices[] = {
                    3, 2, 0,
                    0, 1, 3,

                    7, 5, 4,
                    7, 4, 6,

                    10, 11, 8,
                    8, 11, 9,

                    13, 15, 12,
                    12, 15, 14,

                    19, 18, 16,
                    19, 16, 17,

                    21, 23, 20,
                    20, 23, 22,
                };

                bufferContainer[INDEX_CUBE] = createIndexBuffer(cubeIndices, sizeof(cubeIndices));
            }

        //    {
        //        //loadFile("asset/model/bun_zipper.ply", VERTEX_OBJ, VERTEX_OBJ_NORM, INDEX_OBJ);
        //        //loadFile("asset/model/dragon_vrip.ply", VERTEX_OBJ, VERTEX_OBJ_NORM, INDEX_OBJ);
        //    }
        //}

        ////create constant buffer
        //{
        //    //create projection buffer
        //    {
        //        bufferContainer[CONSTANT_PROJECTION] = createConstantBuffer(sizeof(float) * (4 * 4 * 2 + 3));
        //    }

        //    //create object buffer
        //    {
        //        bufferContainer[CONSTANT_OBJECT] = createConstantBuffer(sizeof(float) * (4 * 4 + 3 + 1 + 1));
        //        bufferContainer[CONSTANT_OBJECT2] = createConstantBuffer(sizeof(float) * (4 * 4 + 3 + 1 + 1));
        //    }
        //    
        //    {
        //        bufferContainer[CONSTANT_SUN] = createConstantBuffer(sizeof(float) * (3));
        //    }

        //    //create hamsley random buffer
        //    {
        //        bufferContainer[CONSTANT_HAMRAN] = createConstantBuffer(sizeof(float) * 2 * 100 + sizeof(float));
        //    }
        //}

        //create depth buffer
        {
            uint width = e_globWindow.width();
            uint height = e_globWindow.height();

            {
                bufferContainer[DEPTH_SWAPCHAIN] = createDepthBuffer(width, height);
            }
        }

        //create image buffer
        {
            uint width = e_globWindow.width();
            uint height = e_globWindow.height();

            bufferContainer[IMAGE_DEPTH] = createImageBuffer(width, height, 1, DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

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
        for (auto buffer : trackedBuffer)
        {
            delete buffer;
        }
        trackedBuffer.clear();
    }

    vertexbuffer* createVertexBuffer(void* vert, const uint size, const uint stride)
	{
        vertexbuffer* newVertexBuffer = new vertexbuffer();

        trackedBuffer.push_back(newVertexBuffer);

        createResource(newVertexBuffer, size, vert);

        // Initialize the vertex buffer view.
        newVertexBuffer->view.BufferLocation = newVertexBuffer->resource->GetGPUVirtualAddress();
        newVertexBuffer->view.StrideInBytes = stride;
        newVertexBuffer->view.SizeInBytes = size;
    
		return newVertexBuffer;
	}

    vertexbuffer* createVertexBufferFromUAV(uavbuffer* uav, const uint stride)
    {
        vertexbuffer* newVertexBuffer = new vertexbuffer();

        newVertexBuffer->resource = uav->resource;

        trackedBuffer.push_back(newVertexBuffer);

        // Initialize the vertex buffer view.
        newVertexBuffer->view.BufferLocation = uav->resource->GetGPUVirtualAddress();
        newVertexBuffer->view.StrideInBytes = stride;
        newVertexBuffer->view.SizeInBytes = uav->view.Buffer.NumElements * sizeof(float);

        return newVertexBuffer;
    }

    vertexbuffer* getVertexBuffer(int index)
	{
        //TC_ASSERT(index < VERTEX_END && index >= VERTEX_START);

		return dynamic_cast<vertexbuffer*>(bufferContainer[index]);
	}

    constantbuffer* createConstantBuffer(const uint size)
    {
        //cbv size should be multiplied by 256 bytes
        uint paddingSize = size;
        if(size % CBVALLIGNMENT != 0) paddingSize  = ((size / CBVALLIGNMENT) + 1) * CBVALLIGNMENT;

        constantbuffer* newBuffer = new constantbuffer();

        trackedBuffer.push_back(newBuffer);

        createResource(newBuffer, paddingSize, D3D12_RESOURCE_FLAG_NONE);

        newBuffer->view.BufferLocation = newBuffer->resource->GetGPUVirtualAddress();
        newBuffer->view.SizeInBytes = paddingSize;

        CD3DX12_RANGE readRange(0, 0);
        newBuffer->resource->Map(0, &readRange, reinterpret_cast<void**>(&newBuffer->info.cbvDataBegin));

        newBuffer->info.size = size;

        return newBuffer;
    }

    constantbuffer* createConstantBufferFromVertex(vertexbuffer* vert)
    {
        constantbuffer* newBuffer = new constantbuffer();

        trackedBuffer.push_back(newBuffer);

        newBuffer->resource = vert->resource;

        newBuffer->view.BufferLocation = vert->resource->GetGPUVirtualAddress();
        newBuffer->view.SizeInBytes = vert->view.SizeInBytes;

        CD3DX12_RANGE readRange(0, 0);
        newBuffer->resource->Map(0, &readRange, reinterpret_cast<void**>(&newBuffer->info.cbvDataBegin));

        newBuffer->info.size = vert->view.SizeInBytes;

        return newBuffer;
    }

    constantbuffer* createConstantBufferFromIndex(indexbuffer* index)
    {
        constantbuffer* newBuffer = new constantbuffer();

        trackedBuffer.push_back(newBuffer);

        newBuffer->resource = index->resource;

        newBuffer->view.BufferLocation = index->resource->GetGPUVirtualAddress();
        newBuffer->view.SizeInBytes = index->view.SizeInBytes;

        CD3DX12_RANGE readRange(0, 0);
        newBuffer->resource->Map(0, &readRange, reinterpret_cast<void**>(&newBuffer->info.cbvDataBegin));

        newBuffer->info.size = index->view.SizeInBytes;

        return newBuffer;
    }

    constantbuffer* getConstantBuffer(int index)
    {
        TC_ASSERT(index < CONSTANT_END && index >= CONSTANT_START);

        return dynamic_cast<constantbuffer*>(bufferContainer[index]);
    }

    indexbuffer* createIndexBuffer(void* data, const uint size)
    {
        indexbuffer* newBuffer = new indexbuffer();

        trackedBuffer.push_back(newBuffer);

        createResource(newBuffer, size, data);

        // Initialize the vertex buffer view.
        newBuffer->view.BufferLocation = newBuffer->resource->GetGPUVirtualAddress();
        newBuffer->view.SizeInBytes = size;
        newBuffer->view.Format = DXGI_FORMAT_R32_UINT;

        return newBuffer;
    }

    indexbuffer* createIndexBufferFromUAV(uavbuffer* uav)
    {
        indexbuffer* newBuffer = new indexbuffer();

        newBuffer->resource = uav->resource;

        trackedBuffer.push_back(newBuffer);

        // Initialize the vertex buffer view.
        newBuffer->view.BufferLocation = uav->resource->GetGPUVirtualAddress();
        newBuffer->view.SizeInBytes = uav->view.Buffer.NumElements * sizeof(float);
        newBuffer->view.Format = DXGI_FORMAT_R32_UINT;

        return newBuffer;
    }

    indexbuffer* getIndexBuffer(int index)
    {
        TC_ASSERT(index < INDEX_END && index >= INDEX_START);

        return dynamic_cast<indexbuffer*>(bufferContainer[index]);
    }

    depthbuffer* createDepthBuffer(const uint width, const uint height)
    {
        depthbuffer* newBuffer = new depthbuffer();

        trackedBuffer.push_back(newBuffer);

        DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT;

        CD3DX12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(format, 1.0f, 0);

        createTexture(newBuffer, format, width, height, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE, &clearValue, true);

        newBuffer->view.Format = format;
        newBuffer->view.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        newBuffer->view.Flags = D3D12_DSV_FLAG_NONE;

        return newBuffer;
    }

    depthbuffer* getDepthBuffer(int index)
    {
        TC_ASSERT(index < DEPTH_END && index >= DEPTH_START);

        return dynamic_cast<depthbuffer*>(bufferContainer[index]);
    }

    imagebuffer* createImageBuffer(const uint width, const uint height, const uint mipLevel, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
    {
        imagebuffer* newBuffer = new imagebuffer();

        trackedBuffer.push_back(newBuffer);

        if (height == 0)
        {
            createNonTexture(newBuffer, width, flags);
        }
        else
        {
            createTexture(newBuffer, format, width, height, mipLevel, flags, nullptr);
        }

        newBuffer->view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        newBuffer->view.Format = format;

        return newBuffer;
    }

    imagebuffer* createImageBufferFromBuffer(buffer* targetBuffer, D3D12_BUFFER_SRV desc)
    {
        imagebuffer* newBuffer = new imagebuffer();

        trackedBuffer.push_back(newBuffer);

        newBuffer->resource = targetBuffer->resource;

        newBuffer->view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        newBuffer->view.Format = DXGI_FORMAT_UNKNOWN;
        newBuffer->view.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        newBuffer->view.Buffer = desc;

        return newBuffer;
    }

    imagebuffer* getImageBuffer(int index)
    {
        TC_ASSERT(index < IMAGE_END && index >= IMAGE_START);

        return dynamic_cast<imagebuffer*>(bufferContainer[index]);
    }


    uavbuffer* createUAVBuffer(const uint size)
    {
        uavbuffer* newBuffer = new uavbuffer();

        trackedBuffer.push_back(newBuffer);

        createResource(newBuffer, size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        newBuffer->view.Format = DXGI_FORMAT_R32_FLOAT;
        newBuffer->view.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        newBuffer->view.Buffer.FirstElement = 0;
        newBuffer->view.Buffer.NumElements = size / sizeof(float);

        return newBuffer;
    }

    uavbuffer* getUAVBuffer(int index)
    {
        TC_ASSERT(index < UAV_END && index >= UAV_START);

        return dynamic_cast<uavbuffer*>(bufferContainer[index]);
    }

    BUFFER_TYPE typeFromIndex(const uint index)
    {
        if (index < VERTEX_END && index >= VERTEX_START) return BUFFER_VERTEX_TYPE;
        else if (index < CONSTANT_END && index >= CONSTANT_START) return BUFFER_CONSTANT_TYPE;
        else if (index < UAV_END && index >= UAV_START) return BUFFER_UAV_TYPE;
        else if (index < IMAGE_END && index >= IMAGE_START) return BUFFER_IMAGE_TYPE;
        else if (index < INDEX_END && index >= INDEX_START) return BUFFER_INDEX_TYPE;
        else if (index < DEPTH_END && index >= DEPTH_START) return BUFFER_DEPTH_TYPE;

        //this should not happen
        
        return BUFFER_TYPE();
    }

    buffer* createReadBackBuffer(uint size)
    {
        buffer* newBuffer = new uavbuffer();

        CD3DX12_HEAP_PROPERTIES heap_property;
        D3D12_RESOURCE_STATES state;

        heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        state = D3D12_RESOURCE_STATE_COPY_DEST;

        CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

        e_globRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc,
            state, nullptr, IID_PPV_ARGS(&newBuffer->resource));

        return newBuffer;
    }
};

void buffer::uploadBuffer(uint size, void* data)
{
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);
    resource->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    memcpy(pVertexDataBegin, data, size);
    resource->Unmap(0, nullptr);
}

void buffer::mapBuffer(unsigned char** dataPtr)
{
    CD3DX12_RANGE readRange(0, 0);
    resource->Map(0, &readRange, reinterpret_cast<void**>(dataPtr));
}

void buffer::unmapBuffer()
{
    resource->Unmap(0, nullptr);
}

uint vertexbuffer::getElemSize() const
{
    return view.SizeInBytes / view.StrideInBytes;
}

uint indexbuffer::getElemSize() const
{
    uint elemByteSize = 4;

    //TODO
    return view.SizeInBytes / elemByteSize;
}
