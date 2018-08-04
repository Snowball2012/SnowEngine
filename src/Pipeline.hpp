#include "Pipeline.h"

template<template <typename> class ... Node>
void Pipeline<Node...>::RebuildPipeline()
{
	/*
		general scheme:
			make typeid -> input, output, rendernode_ptr
	*/

	NOTIMPL;
	m_need_to_rebuild_pipeline = false;
}

template<template <typename> class ...Node>
inline void Pipeline<Node...>::Run()
{
	NOTIMPL;
}

template<template <typename> class ...Node>
template<template <typename> class N>
N<Pipeline<Node...>>* Pipeline<Node...>::GetNode()
{
	return std::get<NStorage<N<Pipeline>>>( m_node_storage ).node.get_ptr();
}
