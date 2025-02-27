#pragma once

namespace App1
{
	// MVP matrislerini köşe gölgelendiriciye göndermek için kullanılan sabit arabelleği.
	struct ModelViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	// Her köşeye ait verileri köşe gölgelendiricisine göndermek için kullanılır.
	struct VertexPositionColor
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 color;
	};
}