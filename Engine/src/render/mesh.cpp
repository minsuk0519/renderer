#include <render\mesh.hpp>
#include <system\gui.hpp>
#include <render/renderer.hpp>
#include <render/framebuffer.hpp>
#include <render/shader_defines.hpp>

#include <array>
#include <cmath>
#include <algorithm>

#include <d3dx12.h>

namespace msh
{
	std::array<mesh*, MESH_SIZE> meshes;

#if ENGINE_DEBUG_MESH
	static buffer* debugProjectionBuffer;
	static float meshDebugDrawCamArmLength_Default = 2.5f;
	static DirectX::XMVECTOR meshDebugDrawCamPos_Default = DirectX::XMVECTOR{ 0.0f, 0.0f, meshDebugDrawCamArmLength_Default };
	static float meshDebugDrawCamArmLength = meshDebugDrawCamArmLength_Default;
	static DirectX::XMVECTOR meshDebugDrawCamPos = meshDebugDrawCamPos_Default;
	static bool openDebugWindow = false;
#endif // #if ENGINE_DEBUG_MESH

	bool loadResources()
	{
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

            buf::uploadLoadedMesh(newData->getData(), cubeVertices, cubeNorm, cubeIndices, 24, 36, MESH_CUBE);
        }

        {
            mesh* newData = new mesh(MESH_SPHERE);
            
            buf::loadFiletoMesh("data/asset/model/sphere.obj", newData->getData(), MESH_SPHERE);
            
            meshes[MESH_SPHERE] = newData;
        }

		{
			mesh* newData = new mesh(MESH_BUNNY);
            
			buf::loadFiletoMesh("data/asset/model/bunny.obj", newData->getData(), MESH_BUNNY);

			meshes[MESH_BUNNY] = newData;
		}

#if ENGINE_DEBUG_MESH
		debugProjectionBuffer = e_globBufAllocator.alloc(nullptr, consts::CONST_PROJ_SIZE, 1, buf::GBF_CBV, buf::RESOURCE_UPLOAD);
#endif // #if ENGINE_DEBUG_MESH

        return true;
	}

    meshData* setUpTerrain(uint iNum)
    {
        mesh* newData = new mesh(MESH_TERRAIN);
        meshData* newMeshData = new meshData();

        newMeshData->lodNum = 1;
        newMeshData->lodData.push_back(lodInfos(8192, iNum));
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

        return newMeshData;
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

            //if (vertexbuffer* vb = msh->getData()->vbs; vb != nullptr)
            //{
            //    uint channel = vb->view.StrideInBytes / 4;
            //    uint size = vb->view.SizeInBytes / (channel * 4);

            //    std::string str = std::format("Vertex Size : {}, Channel : {}", size, channel);

            //    ImGui::Text(str.c_str());
            //}

            //if (vertexbuffer* norm = msh->getData()->norm; norm != nullptr)
            //{
            //    uint channel = norm->view.StrideInBytes / 4;
            //    uint size = norm->view.SizeInBytes / (channel * 4);

            //    std::string str = std::format("Normal Size : {}, Channel : {}", size, channel);

            //    ImGui::Text(str.c_str());
            //}

            //if (indexbuffer* ib = msh->getData()->idx; ib != nullptr)
            //{
            //    uint size = ib->view.SizeInBytes / 4;

            //    std::string str = std::format("Index Size : {}", size);

            //    ImGui::Text(str.c_str());
            //}


        }
    }

#if ENGINE_DEBUG_MESH
	void guiMeshViewSetting(bool openMeshClick, uint meshID)
	{
		if (openMeshClick)
		{
			e_globRenderer.debugFrameBufferRequest(meshID, debugProjectionBuffer->getDesc(buf::GBF_CBV)->getHandle().ptr);
			openDebugWindow = true;
		}

		if (openDebugWindow)
		{
			framebuffer* fbo = e_globRenderer.getDebugFrameBuffer();
			ImGui::Image((ImTextureID)(fbo->getDescHandle(0).ptr), ImVec2(256.0f, 256.0f), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Border));

			static float x = 0;
			static float y = PI / 2.0f;

			bool changed = false;

			ImGui::SameLine();
			ImGui::BeginChild("ArrowButtons", ImVec2(70.0f, 70.0f));
			ImGui::Columns(3, nullptr, false);
			ImGui::PushButtonRepeat(true);
			for (int i = 0; i < 9; i++)
			{
				if (i == 0) if (ImGui::Button("+##ZoomIn")) { meshDebugDrawCamArmLength -= 0.1f; changed = true; }
				if (i == 1) if (ImGui::ArrowButton("meshView##Up", ImGuiDir_Up)) { y -= 0.1f; changed = true; }
				if (i == 2) if (ImGui::Button("-##ZoomOut")) { meshDebugDrawCamArmLength += 0.1f; changed = true; }
				if (i == 3) if (ImGui::ArrowButton("meshView##Left", ImGuiDir_Left)) { x -= 0.1f; changed = true; }
				if (i == 5) if (ImGui::ArrowButton("meshView##Right", ImGuiDir_Right)) { x += 0.1f; changed = true; }
				if (i == 7) if (ImGui::ArrowButton("meshView##Down", ImGuiDir_Down)) { y += 0.1f; changed = true; }
				ImGui::NextColumn();
			}
			ImGui::PopButtonRepeat();

			ImGui::EndChild();

			if (ImGui::Button("Reset##MeshView"))
			{
				meshDebugDrawCamPos = meshDebugDrawCamPos_Default;
				x = 0;
				y = PI / 2.0f;
			}

			if (y > PI) y = PI - 0.01f;
			if (y < 0.0f) y = 0.01f;

			if(ImGui::Button("Close##MeshView"))
			{
				openDebugWindow = false;
			}

			if (changed || openMeshClick)
			{
				float xPos = std::sinf(x) * std::sinf(y) * meshDebugDrawCamArmLength;
				float yPos = std::cosf(y) * meshDebugDrawCamArmLength;
				float zPos = std::cosf(x) * std::sinf(y) * meshDebugDrawCamArmLength;
				meshDebugDrawCamPos = DirectX::XMVECTOR{ xPos, yPos, zPos };
				e_globRenderer.debugFrameBufferRequest(meshID, debugProjectionBuffer->getDesc(buf::GBF_CBV)->getHandle().ptr);

				DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(DirectX::XMVectorNegate(meshDebugDrawCamPos));
				DirectX::XMVECTOR globUp = DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f };

				DirectX::XMVECTOR right = DirectX::XMVector3Cross(forward, globUp);
				DirectX::XMVECTOR up = DirectX::XMVector3Cross(right, forward);

				DirectX::XMMATRIX view = DirectX::XMMatrixLookToRH(meshDebugDrawCamPos, forward, up);

				DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(45.0f), 1.0f, 0.1f, 10.0f);

				DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, projection);

				float* aabbSize = msh::getMesh(meshID)->getData()->boundData.halfExtent;

				float safeExtent[3] =
				{
					(std::max)(aabbSize[msh::AXIS_X], 1e-6f),
					(std::max)(aabbSize[msh::AXIS_Y], 1e-6f),
					(std::max)(aabbSize[msh::AXIS_Z], 1e-6f)
				};

				debugProjectionBuffer->uploadBuffer(sizeof(float) * 4 * 4, 0, &viewProj);
				debugProjectionBuffer->uploadBuffer(sizeof(float) * 3, sizeof(float) * 4 * 4, safeExtent);
			}
		}
	}

	void closeMeshView()
	{
		openDebugWindow = false;
	}
#endif // #if ENGINE_DEBUG_MESH
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

//TODO : move this to renderer
void mesh::draw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
    if (data)
    {
        cmdList->DrawInstanced(data->lodData[0].totalIndicesCount, 1, 0, 0);
    }
}