#include "stdafx.h"
#include "Resources\Graphics.h"
#include "Resources\DescriptorHeap.h"
#include "RendertargetManager.h"

RendertargetManager::~RendertargetManager()
{
	for (Rendertarget* rt : m_rendertargets)
	{
		delete rt;
	}

	m_rendertargets.clear();
}
