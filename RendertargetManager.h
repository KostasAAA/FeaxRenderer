#pragma once

#include "Resources\Rendertarget.h"
#include<vector>

class RendertargetManager
{
public:

	RendertargetManager() {}
	virtual ~RendertargetManager();

	Rendertarget* Get(Rendertarget::Description& desc) 
	{ 
		Rendertarget* rt = new Rendertarget(desc);
		m_rendertargets.push_back(rt);

		return rt;
	}

private:

	std::vector<Rendertarget*> m_rendertargets;
};


