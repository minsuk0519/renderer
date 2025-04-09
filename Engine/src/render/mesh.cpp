#include <render\mesh.hpp>
#include <system\gui.hpp>

#include <array>

#include <d3dx12.h>

namespace msh
{
	std::array<mesh*, MESH_SIZE> meshes;

	bool loadResources()
	{
        //create triangle vertex
        {
            mesh* newData = new mesh(MESH_SCENE_TRIANGLE);

            float triangleVertices[] =
            {
                -1.0, 3.0,
                3.0, -1.0,
                -1.0, -1.0,
            };

            newData->getData()->vbs = buf::createVertexBuffer(triangleVertices, sizeof(triangleVertices), sizeof(float) * 2);

            meshes[MESH_SCENE_TRIANGLE] = newData;
        }

        //create cube vertex
        {
            mesh* newData = new mesh(MESH_CUBE);
            
            float cubeVertices[] =
            {
                1.0f,  1.0f,  1.0f,
                1.0f, -1.0f,  1.0f,
               -1.0f,  1.0f,  1.0f,
               -1.0f, -1.0f,  1.0f,

                1.0f,  1.0f, -1.0f,
                1.0f, -1.0f, -1.0f,
               -1.0f,  1.0f, -1.0f,
               -1.0f, -1.0f, -1.0f,

                1.0f,  1.0f,  1.0f,
                1.0f,  1.0f, -1.0f,
               -1.0f,  1.0f,  1.0f,
               -1.0f,  1.0f, -1.0f,

                1.0f, -1.0f,  1.0f,
                1.0f, -1.0f, -1.0f,
               -1.0f, -1.0f,  1.0f,
               -1.0f, -1.0f, -1.0f,

                1.0f,  1.0f,  1.0f,
                1.0f,  1.0f, -1.0f,
                1.0f, -1.0f,  1.0f,
                1.0f, -1.0f, -1.0f,

               -1.0f,  1.0f,  1.0f,
               -1.0f, -1.0f,  1.0f,
               -1.0f,  1.0f, -1.0f,
               -1.0f, -1.0f, -1.0f,
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

            newData->getData()->vbs = buf::createVertexBuffer(cubeVertices, sizeof(cubeVertices), sizeof(float) * 3);
            newData->getData()->norm = buf::createVertexBuffer(cubeNorm, sizeof(cubeNorm), sizeof(float) * 3);

            uint cubeIndices[] = {
                3, 0, 2,
                0, 3, 1,

                7, 4, 5,
                7, 6, 4,

                10, 8, 11,
                8, 9, 11,

                13, 12, 15,
                12, 14, 15,

                19, 16, 18,
                19, 17, 16,

                21, 20, 23,
                20, 22, 23,
            };

            uint cubeIndicesLine[] = { 
                3, 2, 2, 0, //0, 3,
                0, 1, 1, 3, //3, 0,

                7, 5, 5, 4, //4, 7,
                /*7, 4,*/ 4, 6, 6, 7, 

                10, 11, /*11, 8,*/ 8, 10,
                /*8, 11,*/ 11, 9, 9, 8,

                13, 15, /*15, 12,*/ 12, 13,
                /*12, 15,*/ 15, 14, 14, 12,

                19, 18, 18, 16, //16, 19,
                /*19, 16,*/ 16, 17, 17, 19,

                21, 23, /*23, 20,*/ 20, 21,
                /*20, 23,*/ 23, 22, 22, 20,
            };

            newData->getData()->idx = buf::createIndexBuffer(cubeIndices, sizeof(cubeIndices));
            newData->getData()->idxLine = buf::createIndexBuffer(cubeIndicesLine, sizeof(cubeIndicesLine));

            //no LOD
            newData->getData()->lodNum = 1;
            newData->getData()->lodData.push_back(lodInfos(1, 36, { 36 }));

            newData->getData()->boundData.halfExtent[msh::AXIS_X] = 1.0f;
            newData->getData()->boundData.halfExtent[msh::AXIS_Y] = 1.0f;
            newData->getData()->boundData.halfExtent[msh::AXIS_Z] = 1.0f;

            newData->getData()->boundData.radius = 1.0f;

            clusterbounddata bound;
            bound.sphere.center[0] = 0.0f;
            bound.sphere.center[1] = 0.0f;
            bound.sphere.center[2] = 0.0f;
            bound.sphere.radius = std::sqrt(3.0f);
            bound.aabb.center[0] = 0.0f;
            bound.aabb.center[1] = 0.0f;
            bound.aabb.center[2] = 0.0f;
            bound.aabb.hExtent[0] = 1.0f;
            bound.aabb.hExtent[1] = 1.0f;
            bound.aabb.hExtent[2] = 1.0f;
            newData->getData()->clusterBounds.push_back(bound);

            meshes[MESH_CUBE] = newData;
        }

        {
            mesh* newData = new mesh(MESH_SPHERE);
            
            buf::loadFiletoMesh("data/asset/model/sphere.obj", newData->getData());
            
            meshes[MESH_SPHERE] = newData;
        }

		{
			mesh* newData = new mesh(MESH_BUNNY);
            
			buf::loadFiletoMesh("data/asset/model/bunny.obj", newData->getData());
            
			meshes[MESH_BUNNY] = newData;
		}

        return true;
	}

    void setUpTerrain(vertexbuffer* vertex, vertexbuffer* n, indexbuffer* index)
    {
        mesh* newData = new mesh(MESH_TERRAIN);
        meshData* newMeshData = new meshData();

        newMeshData->vbs = vertex;
        newMeshData->norm = n;
        newMeshData->idx = index;

        newMeshData->lodNum = 1;
        newMeshData->lodData.push_back(lodInfos(8192, index->view.SizeInBytes / sizeof(uint)));
        for (uint i = 0; i < 8192; ++i)
        {
            newMeshData->lodData[0].indexSize.push_back(192);
        }

        newMeshData->boundData.halfExtent[msh::AXIS_X] = 256.0f;
        newMeshData->boundData.halfExtent[msh::AXIS_Y] = 128.0f;
        newMeshData->boundData.halfExtent[msh::AXIS_Z] = 256.0f;

        newMeshData->boundData.radius = 256.0f;

        newData->init(newMeshData);

        meshes[MESH_TERRAIN] = newData;
    }

    void cleanUp()
    {
        for (uint i = 0; i < MESH_END; ++i)
        {
            if (meshes[i] == nullptr) continue;
            meshes[i]->close();
            delete meshes[i];
        }
    }

    mesh* getMesh(const uint& idx)
    {
        return meshes[idx];
    }

    mesh* getMesh(const MESH_INDEX idx)
    {
        return meshes[idx];
    }

    void guiMeshSetting(bool& openMeshWindow, uint& meshID)
    {
        uint meshSize = MESH_END;

        for (uint i = 0; i < meshSize; ++i)
        {
            mesh* msh = meshes[i];

            std::string nameText = std::format("Name : {}", MESHNAME[i]);

            ImGui::BulletText(nameText.c_str()); ImGui::SameLine();

            if (ImGui::Button(("View##MESH" + std::to_string(i)).c_str()))
            {
                openMeshWindow = true;
                meshID = i;
            }

            if (vertexbuffer* vb = msh->getData()->vbs; vb != nullptr)
            {
                uint channel = vb->view.StrideInBytes / 4;
                uint size = vb->view.SizeInBytes / (channel * 4);

                std::string str = std::format("Vertex Size : {}, Channel : {}", size, channel);

                ImGui::Text(str.c_str());
            }

            if (vertexbuffer* norm = msh->getData()->norm; norm != nullptr)
            {
                uint channel = norm->view.StrideInBytes / 4;
                uint size = norm->view.SizeInBytes / (channel * 4);

                std::string str = std::format("Normal Size : {}, Channel : {}", size, channel);

                ImGui::Text(str.c_str());
            }

            if (indexbuffer* ib = msh->getData()->idx; ib != nullptr)
            {
                uint size = ib->view.SizeInBytes / 4;

                std::string str = std::format("Index Size : {}", size);

                ImGui::Text(str.c_str());
            }


        }
    }
}

mesh::mesh(int index)
{
    data = new meshData();

    ID = index;
}

bool mesh::init(meshData* meshdata)
{
	data = meshdata;

    return true;
}

void mesh::close()
{
    if(data != nullptr) delete data;
}

int mesh::getId() const
{
    if (static_cast<uint>(msh::MESH_SIZE) <= ID)
    {
        return -1;
    }
    return ID;
}

meshData* mesh::getData() const
{
    return data;
}

void mesh::setBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool lineDraw)
{
    cmdList->IASetVertexBuffers(0, 1, &data->vbs->view);

    //TODO : change the location of the vertex buffer (may be in pipeline?)
    if (data->norm) cmdList->IASetVertexBuffers(1, 1, &data->norm->view);

    if (lineDraw)
    {
        cmdList->IASetIndexBuffer(&data->idxLine->view);
        indexCount = data->idxLine->view.SizeInBytes / sizeof(uint);
    }
    else
    {
        cmdList->IASetIndexBuffer(&data->idx->view);
        indexCount = data->idx->view.SizeInBytes / sizeof(uint);
    }

}

void mesh::draw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
    cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}