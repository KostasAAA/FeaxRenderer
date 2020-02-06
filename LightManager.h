#pragma once

#include<vector>
#include<map>
#include<string>

struct Light
{
	union
	{
		XMVECTOR m_position;
		XMVECTOR m_direction;
	};
	XMVECTOR m_colour;
	XMVECTOR m_shadowmapRange;
	float m_attenuation;
	float m_radius;
	bool m_castShadows;
	float m_rotation;
	float m_rotationSpeed;
	float m_intensity;
	ModelInstance* m_lightMesh;
};

class LightManager
{
public:
	LightManager();
	virtual ~LightManager();

	void Update();

	void AddDirectionalLight(Light& light)
	{
		m_directional.push_back(light);
	}

	void AddPointLight(Light& light)
	{
		m_point.push_back(light);
	}

	std::vector<Light>& GetDirectionalLights()
	{
		return m_directional;
	}

	std::vector<Light>& GetPointLights()
	{
		return m_point;
	}
private:
	std::vector<Light> m_directional;
	std::vector<Light> m_point;
};


