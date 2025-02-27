#pragma once

#include <ppltasks.h>	// create_task için

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Win32 API hatalarını yakalamak için bu satıra bir kesme noktası koyun.
			throw Platform::Exception::CreateException(hr);
		}
	}

	// İkili bir dosyadan zaman uyumsuz okuma yapan işlev.
	inline Concurrency::task<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
	{
		using namespace Windows::Storage;
		using namespace Concurrency;

		auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

		return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([] (StorageFile^ file) 
		{
			return FileIO::ReadBufferAsync(file);
		}).then([] (Streams::IBuffer^ fileBuffer) -> std::vector<byte> 
		{
			std::vector<byte> returnBuffer;
			returnBuffer.resize(fileBuffer->Length);
			Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer.data(), fileBuffer->Length));
			return returnBuffer;
		});
	}

	// Cihazdan bağımsız piksel (DIP) birimindeki uzunluğu fiziksel piksel birimine çevirir.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // En yakın tamsayıya yuvarlar.
	}

#if defined(_DEBUG)
	// SDK Katmanı desteği olup olmadığını kontrol edin.
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // Gerçek bir donanım cihazı oluşturmaya gerek yoktur.
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // SDK katmanlarını kontrol edin.
			nullptr,                    // Herhangi bir özellik düzeyi kullanılabilir.
			0,
			D3D11_SDK_VERSION,          // Microsoft Store uygulamaları için bunu her zaman D3D11_SDK_VERSION olarak ayarlayın.
			nullptr,                    // D3D cihaz başvurusunu korumanız gerekmez.
			nullptr,                    // Özellik düzeyini bilmeniz gerekmez.
			nullptr                     // D3D cihaz bağlamı başvurusunu korumanız gerekmez.
			);

		return SUCCEEDED(hr);
	}
#endif
}
