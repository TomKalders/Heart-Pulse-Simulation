#include "Mesh.h"
#include "fstream"

//External Header
#pragma warning(push)
#pragma warning(disable:4244)
#pragma warning(disable:4701)
#include "OBJ_Loader.h"
#undef OBJL_CONSOLE_OUTPUT
#pragma warning(pop)


Mesh::Mesh(ID3D11Device* pDevice, const std::vector<Vertex_Input>& vertices, const std::vector<uint32_t>& indices)
	: m_pEffect{}
	, m_pVertexLayout{nullptr}
	, m_pVertexBuffer{nullptr}
	, m_pIndexBuffer{nullptr}
	, m_AmountIndices{}
	, m_pResourceView{nullptr}
	, m_WorldMatrix{ glm::mat4{1.f} }
{
	CreateEffect(pDevice);
	CreateDirectXResources(pDevice, vertices, indices);
}

Mesh::Mesh(ID3D11Device* pDevice, const std::string& filepath)
	: m_pEffect{}
	, m_pVertexLayout{ nullptr }
	, m_pVertexBuffer{ nullptr }
	, m_pIndexBuffer{ nullptr }
	, m_AmountIndices{}
	, m_pResourceView{ nullptr }
	, m_WorldMatrix{ glm::mat4{1.f} }
{
	CreateEffect(pDevice);
	LoadMeshFromOBJ(filepath);
	CreateDirectXResources(pDevice, m_VertexBuffer, m_IndexBuffer);
}

Mesh::~Mesh()
{
	if (m_pIndexBuffer)
		m_pIndexBuffer->Release();

	if (m_pVertexBuffer)
		m_pVertexBuffer->Release();

	if (m_pVertexLayout)
		m_pVertexLayout->Release();

	if (m_pEffect)
		delete m_pEffect;

}

void Mesh::Render(ID3D11DeviceContext* pDeviceContext, const float* worldViewProjMatrix, const float* inverseView)
{
	//Set vertex buffer
	UINT stride = sizeof(Vertex_Input);
	UINT offset = 0;
	pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

	//Set index buffer
	pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	//Set input layout
	pDeviceContext->IASetInputLayout(m_pVertexLayout);

	//Set primitive topology
	pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//Set the worldviewprojectionMatrix
	m_pEffect->GetWorldViewProjMatrix()->SetMatrix(worldViewProjMatrix);

	//Set the worldMatrix
	glm::mat4 world = glm::transpose(m_WorldMatrix);
	//glm::mat4 world = m_WorldMatrix;
	
	//float* data = (float*)(world[0].x);
	float* data = (float*)glm::value_ptr(world);
	m_pEffect->GetWorldMatrix()->SetMatrix(data);

	//Set the InverseViewMatrix
	m_pEffect->GetViewInverseMatrix()->SetMatrix(inverseView);

	//Render triangle
	D3DX11_TECHNIQUE_DESC techDesc;
	m_pEffect->GetTechnique()->GetDesc(&techDesc);
	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		m_pEffect->GetTechnique()->GetPassByIndex(p)->Apply(0, pDeviceContext);
		pDeviceContext->DrawIndexed(m_AmountIndices, 0, 0);
	}
}

glm::mat4 Mesh::GetWorldMatrix()
{
	return m_WorldMatrix;
}

const std::vector<uint32_t>& Mesh::GetIndexBuffer()
{
	return m_IndexBuffer;
}

const std::vector<Vertex_Input>& Mesh::GetVertexBuffer()
{
	return m_VertexBuffer;
}

HRESULT Mesh::CreateDirectXResources(ID3D11Device* pDevice, const std::vector<Vertex_Input>& vertices, const std::vector<uint32_t>& indices)
{
	HRESULT result = S_OK;

	if (m_pIndexBuffer)
		m_pIndexBuffer->Release();

	if (m_pVertexBuffer)
		m_pVertexBuffer->Release();

	if (m_pVertexLayout)
		m_pVertexLayout->Release();

	//Create vertex layout
	static const uint32_t numElements{ 6 };
	D3D11_INPUT_ELEMENT_DESC vertexDesc[numElements]{};

	vertexDesc[0].SemanticName = "POSITION";
	vertexDesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	vertexDesc[0].AlignedByteOffset = 0;
	vertexDesc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	vertexDesc[1].SemanticName = "COLOR";
	vertexDesc[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	vertexDesc[1].AlignedByteOffset = 12;
	vertexDesc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	vertexDesc[2].SemanticName = "NORMAL";
	vertexDesc[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	vertexDesc[2].AlignedByteOffset = 24;
	vertexDesc[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	vertexDesc[3].SemanticName = "TANGENT";
	vertexDesc[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	vertexDesc[3].AlignedByteOffset = 36;
	vertexDesc[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	vertexDesc[4].SemanticName = "TEXCOORD";
	vertexDesc[4].Format = DXGI_FORMAT_R32G32_FLOAT;
	vertexDesc[4].AlignedByteOffset = 44;
	vertexDesc[4].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	vertexDesc[5].SemanticName = "POWER";
	vertexDesc[5].Format = DXGI_FORMAT_R32_FLOAT;
	vertexDesc[5].AlignedByteOffset = 48;
	vertexDesc[5].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	//vertexDesc[3].SemanticName = "TEXCOORD";
	//vertexDesc[3].Format = DXGI_FORMAT_R32G32_FLOAT;
	//vertexDesc[3].AlignedByteOffset = 36;
	//vertexDesc[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	//Create the input layout
	D3DX11_PASS_DESC passDesc;
	m_pEffect->GetTechnique()->GetPassByIndex(0)->GetDesc(&passDesc);
	result = pDevice->CreateInputLayout(
		vertexDesc,
		numElements,
		passDesc.pIAInputSignature,
		passDesc.IAInputSignatureSize,
		&m_pVertexLayout
	);

	//Create vertex buffer
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.ByteWidth = sizeof(Vertex_Input) * (uint32_t)vertices.size();
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA initData = { 0 };
	initData.pSysMem = vertices.data();
	result = pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer);
	if (FAILED(result))
		return result;

	//Create index buffer
	m_AmountIndices = (uint32_t)indices.size();
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.ByteWidth = sizeof(uint32_t) * m_AmountIndices;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	initData.pSysMem = indices.data();
	result = pDevice->CreateBuffer(&bd, &initData, &m_pIndexBuffer);
	if (FAILED(result))
		return result;

	return result;
}

void Mesh::LoadMeshFromOBJ(const std::string& pathName)
{
	objl::Loader loader;

	bool loadout = loader.LoadFile(pathName);
	if (loadout)
	{
		if (loader.LoadedMeshes.size() > 0)
		{
			for (const objl::Vertex& vertex : loader.LoadedVertices)
			{
				m_VertexBuffer.push_back({
					{ vertex.Position.X, vertex.Position.Y, vertex.Position.Z },
					{ 1.f, 1.f, 1.f},
					{ vertex.Normal.X, vertex.Normal.Y, vertex.Normal.Z },
					{ vertex.TextureCoordinate.X, vertex.TextureCoordinate.Y }
				});
			}

			for (unsigned int index : loader.LoadedIndices)
			{
				m_IndexBuffer.push_back(index);
			}

			for (int i = 0; i < m_IndexBuffer.size(); i++)
			{
				if (i % 3 == 2)
				{
					std::swap(m_IndexBuffer[i - 1], m_IndexBuffer[i]);
				}

				if (i % 3 == 0)
				{
					int index0 = m_IndexBuffer[i];
					int index1 = m_IndexBuffer[i + 1];
					int index2 = m_IndexBuffer[i + 2];
					Vertex_Input& vertex0 = m_VertexBuffer[index0];
					Vertex_Input& vertex1 = m_VertexBuffer[index1];
					Vertex_Input& vertex2 = m_VertexBuffer[index2];

					const glm::fvec3 edge0 = (vertex1.position - vertex0.position);
					const glm::fvec3 edge1 = (vertex2.position - vertex0.position);

					const glm::fvec2 diffX = glm::fvec2(vertex1.uv.x - vertex0.uv.x, vertex2.uv.x - vertex0.uv.x);
					const glm::fvec2 diffY = glm::fvec2(vertex1.uv.y - vertex0.uv.y, vertex2.uv.y - vertex0.uv.y);
					const float r = 1.f / (diffX.x * diffY.y - diffX.y * diffY.x)/*glm::cross(diffX, diffY)*/;

					glm::vec3 tangent = (edge0 * diffY.y - edge1 * diffY.x) * r;
					//tangent.z = -tangent.z;
					vertex0.tangent = tangent;
					vertex1.tangent = tangent;
					vertex2.tangent = tangent;
				}
			}
		}
	}
}


void Mesh::CreateEffect(ID3D11Device* pDevice/*, bool isTransparent*/)
{

	m_pEffect = new BaseEffect(pDevice, L"Resources/Shader/PosCol.fx");
}