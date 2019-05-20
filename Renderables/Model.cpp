#include "stdafx.h"
#include "Mesh.h"
#include "Model.h"
#include "..\Resources\Texture.h"
#include "..\DXSample.h"

Model::~Model()
{
	for (Mesh *mesh : m_meshes)
	{
		delete mesh;
	}
}

void Model::Render(ID3D12GraphicsCommandList* commandList)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	for (Mesh *mesh : m_meshes)
	{
		commandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
		commandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
		commandList->DrawIndexedInstanced(mesh->GetNoofIndices(), 1, 0, 0, 0);
	}
}