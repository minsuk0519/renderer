#include <render\mesh.hpp>

#include <array>

#include <d3dx12.h>

namespace msh
{
	std::array<mesh*, MESH_END> meshes;

	bool loadResources()
	{
        //create triangle vertex
        {
            mesh* newData = new mesh(MESH_TRIANGLE);

            float triangleVertices[] =
            {
                0.0, 0.5, 0.0f,
                0.5, -0.5, 0.0f,
                0.0, -0.5, 0.0f,
            };

            newData->getData()->vbs = buf::createVertexBuffer(triangleVertices, sizeof(triangleVertices), sizeof(float) * 3);

            meshes[MESH_TRIANGLE] = newData;
        }

        //create cube vertex
        {
            mesh* newData = new mesh(MESH_CUBE);
            
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

            meshes[MESH_CUBE] = newData;
        }

        //create cube vertex
        {
            mesh* newData = new mesh(MESH_CUBE_NONORM);

            newData->getData()->vbs = meshes[MESH_CUBE]->getData()->vbs;
            newData->getData()->idx = meshes[MESH_CUBE]->getData()->idx;
            newData->getData()->idxLine = meshes[MESH_CUBE]->getData()->idxLine;

            meshes[MESH_CUBE_NONORM] = newData;
        }

        {
            //mesh* newData = new mesh(MESH_SPHERE);
            //
            //buf::loadFiletoMesh("data/asset/model/sphere.obj", newData->getData());
            //
            //meshes[MESH_SPHERE] = newData;
        }

		{
			mesh* newData = new mesh(MESH_BUNNY);
            
			buf::loadFiletoMesh("data/asset/model/bunny.obj", newData->getData());
            
			meshes[MESH_BUNNY] = newData;
		}
		
		{
			//mesh* newData = new mesh(MESH_DRAGON);
			
            //file is too big
			//buf::loadFiletoMesh("asset/model/dragon_vrip.ply", newData->getData());
			
			//meshes[MESH_DRAGON] = newData;
		}

        {
            //mesh* newData = new mesh(MESH_ARMADILLO);

            ////file is too big
            //buf::loadFiletoMesh("asset/model/Armadillo.ply", newData->getData());

            //meshes[MESH_ARMADILLO] = newData;
        }


        {
            //mesh* newData = new mesh(MESH_HAPPY);

            //file is too big
            //buf::loadFiletoMesh("asset/model/happy_vrip.ply", newData->getData());

            //meshes[MESH_HAPPY] = newData;
        }

        {
            mesh* newData = new mesh(MESH_TERRAIN);

            meshes[MESH_TERRAIN] = newData;
        }

        return true;
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

    mesh* getMesh(const MESH_INDEX idx)
    {
        return meshes[idx];
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
    if (static_cast<uint>(msh::MESH_CANNOTLOAD) <= ID)
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