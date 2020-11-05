/**
	Contains the definition of basic vertex data.

	@file Vertex.h
	@author Rudy Zhang
*/

#pragma once

#include <DirectXMath.h>

// --------------------------------------------------------
// A custom vertex definition
//
// --------------------------------------------------------
struct Vertex
{
	DirectX::XMFLOAT3 Position;	    // The position of the vertex
	DirectX::XMFLOAT4 Color;        // The color of the vertex
};