#include "stdafx.h"
#include "Renderables/Model.h"
#include "LightManager.h"


LightManager::LightManager()
{

}

void LightManager::Update()
{
	auto& pointLights = GetPointLights();

	for (Light& light : pointLights)
	{
		if (light.m_position.m128_f32[1] > 5)
		{
			light.m_position.m128_f32[1] = 0;
		}

		XMMATRIX rotationLight = XMMatrixRotationY(light.m_rotationSpeed);
		XMMATRIX translationLight = XMMatrixTranslation( 0.0f, 0.05f, 0.0f );

		rotationLight = translationLight * rotationLight;

		light.m_position = XMVector4Transform(light.m_position, rotationLight);
		
		XMMATRIX objectToWorld = XMMatrixScaling(0.05f, 0.05f, 0.05f) * XMMatrixTranslationFromVector(light.m_position);

		//update attached mesh as well
		light.m_lightMesh->SetMatrix(objectToWorld);
		light.m_lightMesh->Update();
	}
}

LightManager::~LightManager()
{

}

