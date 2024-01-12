#include <render/buffer.hpp>
#include <render/commandqueue.hpp>
#include <render/renderer.hpp>
#include <system/window.hpp>

#include <vector>

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXTex.h>

//#include <rapidobj/rapidobj.hpp>

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

    //vertindex processMesh()//, float AABB[msh::EDGE_MAX])
    //{
    //    //vertindex data;

    //    //float xMAX = -FLT_MAX;
    //    //float yMAX = -FLT_MAX;
    //    //float zMAX = -FLT_MAX;

    //    //float xMIN = FLT_MAX;
    //    //float yMIN = FLT_MAX;
    //    //float zMIN = FLT_MAX;

    //    //std::vector<std::pair<uint, uint>> overlappedVerts;

    //    //for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    //    //{
    //    //    float x = mesh->mVertices[i].x;
    //    //    float y = mesh->mVertices[i].y;
    //    //    float z = mesh->mVertices[i].z;

    //    //    if (xMAX < x) xMAX = x;
    //    //    if (yMAX < y) yMAX = y;
    //    //    if (zMAX < z) zMAX = z;

    //    //    if (xMIN > x) xMIN = x;
    //    //    if (yMIN > y) yMIN = y;
    //    //    if (zMIN > z) zMIN = z;

    //    //    data.vertices.push_back(x);
    //    //    data.vertices.push_back(y);
    //    //    data.vertices.push_back(z);

    //    //    if (mesh->HasNormals())
    //    //    {
    //    //        data.normals.push_back(mesh->mNormals[i].x);
    //    //        data.normals.push_back(mesh->mNormals[i].y);
    //    //        data.normals.push_back(mesh->mNormals[i].z);
    //    //    }

    //    //    //if (mesh->HasTextureCoords(i))
    //    //    //{
    //    //    //    data.vertices.push_back(mesh->mTextureCoords[i]->x);
    //    //    //    data.vertices.push_back(mesh->mTextureCoords[i]->y);
    //    //    //}
    //    //    //else
    //    //    //{
    //    //    //    data.vertices.push_back(0.0f);
    //    //    //    data.vertices.push_back(0.0f);
    //    //    //}
    //    //}

    //    //float xDIM = xMAX - xMIN;
    //    //float yDIM = yMAX - yMIN;
    //    //float zDIM = zMAX - zMIN;

    //    //float MAXDIM = (xDIM > yDIM) ? xDIM : yDIM;
    //    //MAXDIM = (MAXDIM > zDIM) ? MAXDIM : zDIM;

    //    //float xOFFSET = (xMAX + xMIN) / 2.0f;
    //    //float yOFFSET = (yMAX + yMIN) / 2.0f;
    //    //float zOFFSET = (zMAX + zMIN) / 2.0f;
    //    //    
    //    //for (uint i = 0; i < data.vertices.size(); i += 3)
    //    //{
    //    //    data.vertices[i] = (data.vertices[i] - xOFFSET) / (MAXDIM);
    //    //    data.vertices[i + 1] = (data.vertices[i + 1] - yOFFSET) / (MAXDIM);
    //    //    data.vertices[i + 2] = (data.vertices[i + 2] - zOFFSET) / (MAXDIM);
    //    //}

    //    //xMAX = (xMAX - xOFFSET) / (MAXDIM);
    //    //xMIN = (xMIN - xOFFSET) / (MAXDIM);
    //    //yMAX = (yMAX - yOFFSET) / (MAXDIM);
    //    //yMIN = (yMIN - yOFFSET) / (MAXDIM);
    //    //zMAX = (zMAX - zOFFSET) / (MAXDIM);
    //    //zMIN = (zMIN - zOFFSET) / (MAXDIM);

    //    ////AABB[msh::EDGE_XMAX] = xMAX;
    //    ////AABB[msh::EDGE_XMIN] = xMIN;
    //    ////AABB[msh::EDGE_YMAX] = yMAX;
    //    ////AABB[msh::EDGE_YMIN] = yMIN;
    //    ////AABB[msh::EDGE_ZMAX] = zMAX;
    //    ////AABB[msh::EDGE_ZMIN] = zMIN;

    //    //return data;
    //}

    //void processNode(aiNode* node, const aiScene* scene, std::vector<vertindex>& data, float AABB[msh::EDGE_MAX])
    //{
    //    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    //    {
    //        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
    //        //TODO
    //        data.push_back(processMesh(mesh, scene, AABB));
    //    }

    //    for (unsigned int i = 0; i < node->mNumChildren; i++)
    //    {
    //        processNode(node->mChildren[i], scene, data, AABB);
    //    }
    //}

    //std::vector<vertindex> readassimp(std::string file_path, float AABB[msh::EDGE_MAX])
    //{
    //    Assimp::Importer import;
    //    const aiScene* scene = import.ReadFile(file_path.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals);

    //    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) return {};

    //    std::vector<vertindex> data;

    //    processNode(scene->mRootNode, scene, data, AABB);

    //    return data;
    //}
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
 
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediates;

    void loadFile(std::string fileName, const uint vertexIndex, const uint normalIndex, const uint indexIndex)
    {
        //float AABB[msh::EDGE_MAX];

        //std::vector<meshloadhelper::vertindex> data = meshloadhelper::readassimp(fileName, AABB);

        ////TODO
        //bufferContainer[vertexIndex] = createVertexBuffer(data[0].vertices.data(), static_cast<uint>(sizeof(float) * data[0].vertices.size()), sizeof(float) * 3);
        //bufferContainer[normalIndex] = createVertexBuffer(data[0].normals.data(), static_cast<uint>(sizeof(float) * data[0].normals.size()), sizeof(float) * 3);
        //bufferContainer[indexIndex] = createIndexBuffer(data[0].indices.data(), static_cast<uint>(sizeof(uint) * data[0].indices.size()));
    }

    void loadFiletoMesh(std::string fileName, meshData* meshdata)
    {
        //std::vector<meshloadhelper::vertindex> data = meshloadhelper::readassimp(fileName, meshdata->AABB);

        ////TODO
        //meshdata->vbs = createVertexBuffer(data[0].vertices.data(), static_cast<uint>(sizeof(float) * data[0].vertices.size()), sizeof(float) * 3);
        //if (data[0].normals.size() != 0) meshdata->norm = createVertexBuffer(data[0].normals.data(), static_cast<uint>(sizeof(float) * data[0].normals.size()), sizeof(float) * 3);
        //else meshdata->norm = nullptr;
        //meshdata->idx = createIndexBuffer(data[0].indices.data(), static_cast<uint>(sizeof(uint) * data[0].indices.size()));
        //meshdata->idxLine = createIndexBuffer(data[0].indicesLine.data(), static_cast<uint>(sizeof(uint) * data[0].indicesLine.size()));
    }

    imagebuffer* loadTextureFromFile(std::wstring filename, bool mip)
    {
        DirectX::TexMetadata metadata;
        DirectX::ScratchImage scratchImage;
        DirectX::ScratchImage mipImage;

        DirectX::LoadFromHDRFile(filename.c_str(), &metadata, scratchImage);


        //we assume there will be only one image from one file
        const DirectX::Image* image = scratchImage.GetImages();

        uint maxDim = max(static_cast<uint>(image->width), static_cast<uint>(image->height));

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

            cmdqueue::getCmdQueue(cmdqueue::QUEUE_COPY)->getAllocator()->Reset();
            cmdList->Reset(cmdqueue::getCmdQueue(cmdqueue::QUEUE_COPY)->getAllocator().Get(), nullptr);
            //cmdList->Close();
            //// The "before" state is not important. It will be resolved by the resource state tracker.
            //CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer->resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
            //cmdList->ResourceBarrier(1, &barrier);
        
            UINT64 requiredSize = GetRequiredIntermediateSize(buffer->resource.Get(), 0, static_cast<uint>(subresources.size()));
            
            // Create a temporary (intermediate) resource for uploading the subresources
            Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
            
            CD3DX12_HEAP_PROPERTIES heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
            
            e_GlobRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediateResource));
            
            UpdateSubresources(cmdList.Get(), buffer->resource.Get(), intermediateResource.Get(), 0, 0, static_cast<uint>(subresources.size()), subresources.data());

            intermediates.push_back(intermediateResource);
        }

        cmdqueue::getCmdQueue(cmdqueue::QUEUE_COPY)->execute({ cmdList.Get() });

        cmdqueue::getCmdQueue(cmdqueue::QUEUE_COPY)->flush();
        intermediates.clear();

        return buffer;
    }
}

namespace buf
{
    bool createResource(buffer* buf, uint size, void* data)
    {
        CD3DX12_HEAP_PROPERTIES heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

        e_GlobRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf->resource));

        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);
        buf->resource->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
        memcpy(pVertexDataBegin, data, size);
        buf->resource->Unmap(0, nullptr);

        return true;
    }

    bool createResource(buffer* buf, uint size, D3D12_RESOURCE_FLAGS flags)
    {
        CD3DX12_HEAP_PROPERTIES heap_property;
        D3D12_RESOURCE_STATES state;

        if (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
        {
            heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            //heap_property.CPUPageProperty |= 
            state = D3D12_RESOURCE_STATE_COMMON;
        }
        else
        {
            heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            state = D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        bufDesc.Flags |= flags;

        e_GlobRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc,
            state, nullptr, IID_PPV_ARGS(&buf->resource));

        return true;
    }

    bool createTexture(buffer* buf, DXGI_FORMAT format, uint width, uint height, unsigned int mipLevel, D3D12_RESOURCE_FLAGS flags, CD3DX12_CLEAR_VALUE* clear, bool depth = false)
    {
        CD3DX12_HEAP_PROPERTIES heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, (UINT16)mipLevel, 1, 0, flags);
        D3D12_RESOURCE_STATES resourceStates = D3D12_RESOURCE_STATE_COMMON;
        if (depth) resourceStates = D3D12_RESOURCE_STATE_DEPTH_WRITE;

        //TODO depth_write will be changed
        e_GlobRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc,
            resourceStates, clear, IID_PPV_ARGS(&buf->resource));

        return true;
    }

	bool loadResources(Microsoft::WRL::ComPtr<ID3D12Device2> devicePtr, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> copyCmdList)
	{
        //device = devicePtr;
        //cmdList = copyCmdList;

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
            //cmdqueue::getCmdQueue(cmdqueue::QUEUE_COPY)->execute({ cmdList.Get() });
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

    vertexbuffer* createVertexBuffer(void* data, const uint size, const uint stride)
	{
        vertexbuffer* newVertexBuffer = new vertexbuffer();

        trackedBuffer.push_back(newVertexBuffer);

        createResource(newVertexBuffer, size, data);

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
        //assert(index < VERTEX_END && index >= VERTEX_START);

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
        //assert(index < CONSTANT_END && index >= CONSTANT_START);

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
        //assert(index < INDEX_END && index >= INDEX_START);

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
        //assert(index < DEPTH_END && index >= DEPTH_START);

        return dynamic_cast<depthbuffer*>(bufferContainer[index]);
    }

    imagebuffer* createImageBuffer(const uint width, const uint height, const uint mipLevel, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
    {
        imagebuffer* newBuffer = new imagebuffer();

        trackedBuffer.push_back(newBuffer);

        createTexture(newBuffer, format, width, height, mipLevel, flags, nullptr);

        newBuffer->view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        newBuffer->view.Format = format;
        newBuffer->view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        newBuffer->view.Texture2D.MipLevels = mipLevel;

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
        //assert(index < IMAGE_END && index >= IMAGE_START);

        return dynamic_cast<imagebuffer*>(bufferContainer[index]);
    }


    uavbuffer* createUAVBuffer(const uint size)
    {
        //cbv size should be multiplied by 256 bytes
        uint paddingSize = size;

        uavbuffer* newBuffer = new uavbuffer();

        trackedBuffer.push_back(newBuffer);

        createResource(newBuffer, paddingSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        newBuffer->view.Format = DXGI_FORMAT_R32_FLOAT;
        newBuffer->view.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        newBuffer->view.Buffer.FirstElement = 0;
        newBuffer->view.Buffer.NumElements = size / sizeof(float);

        return newBuffer;
    }

    uavbuffer* getUAVBuffer(int index)
    {
        //assert(index < UAV_END && index >= UAV_START);

        return dynamic_cast<uavbuffer*>(bufferContainer[index]);
    }

    //BUFFER_TYPE typeFromIndex(const uint index)
    //{
    //    if (index < VERTEX_END && index >= VERTEX_START) return BUFFER_VERTEX_TYPE;
    //    else if (index < CONSTANT_END && index >= CONSTANT_START) return BUFFER_CONSTANT_TYPE;
    //    else if (index < UAV_END && index >= UAV_START) return BUFFER_UAV_TYPE;
    //    else if (index < IMAGE_END && index >= IMAGE_START) return BUFFER_IMAGE_TYPE;
    //    else if (index < INDEX_END && index >= INDEX_START) return BUFFER_INDEX_TYPE;
    //    else if (index < DEPTH_END && index >= DEPTH_START) return BUFFER_DEPTH_TYPE;

    //    //this should not happen
    //    
    //    return BUFFER_TYPE();
    //}

    buffer* createReadBackBuffer(uint size)
    {
        buffer* newBuffer = new uavbuffer();

        CD3DX12_HEAP_PROPERTIES heap_property;
        D3D12_RESOURCE_STATES state;

        heap_property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        state = D3D12_RESOURCE_STATE_COPY_DEST;

        CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

        e_GlobRenderer.device->CreateCommittedResource(&heap_property, D3D12_HEAP_FLAG_NONE, &bufDesc,
            state, nullptr, IID_PPV_ARGS(&newBuffer->resource));

        return newBuffer;
    }
};