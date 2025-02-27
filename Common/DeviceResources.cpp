#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include <windows.ui.xaml.media.dxinterop.h>

using namespace D2D1;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;

namespace DisplayMetrics
{
	// Yüksek çözünürlüklü görüntülerin oluşturulması için çok fazla GPU ve pil gücü gerekebilir.
	// Örneğin, yüksek çözünürlüklü telefonlar, oyunların tam uygunlukta saniyede 60 kare hızıyla
	// oluşturmaya çalışması durumunda düşük pil ömrü yaşayabilir.
	// Tüm platformlarda ve form faktörlerinde tam uygunlukta işleme kararı
	// iyi düşünülerek verilmelidir.
	static const bool SupportHighResolutions = false;

	// Bir "yüksek çözünürlüklü" görüntüyü tanımlayan varsayılan eşikler. Eşikler
	// aşılırsa ve SupportHighResolutions değeri false ise boyutlar
	// %50 oranında ölçeklendirilir.
	static const float DpiThreshold = 192.0f;		// Standart masaüstü görüntüsünün %200'ü.
	static const float WidthThreshold = 1920.0f;	// 1080p genişlik.
	static const float HeightThreshold = 1080.0f;	// 1080p yükseklik.
};

// Ekran döndürmelerini hesaplamak için kullanılan sabit değişkenler.
namespace ScreenRotation
{
	// 0 derece Z ekseninde döndürme
	static const XMFLOAT4X4 Rotation0( 
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 90 derece Z ekseninde döndürme
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 180 derece Z ekseninde döndürme
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 270 derece Z ekseninde döndürme
	static const XMFLOAT4X4 Rotation270( 
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// DeviceResources Oluşturucusu.
DX::DeviceResources::DeviceResources() : 
	m_screenViewport(),
	m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_compositionScaleX(1.0f),
	m_compositionScaleY(1.0f),
	m_deviceNotify(nullptr)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Direct3D cihazına bağlı olmayan kaynakları yapılandırır.
void DX::DeviceResources::CreateDeviceIndependentResources()
{
	// Direct2D kaynaklarını başlatın.
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
	// Proje hata ayıklama yapısındaysa SDK Katmanları üzerinden Direct2D hata ayıklamasını etkinleştirin.
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Direct2D Fabrikasını başlatın.
	DX::ThrowIfFailed(
		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory3),
			&options,
			&m_d2dFactory
			)
		);

	// DirectWrite Fabrikasını başlatın.
	DX::ThrowIfFailed(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory3),
			&m_dwriteFactory
			)
		);

	// Windows Imaging Component (WIC) Fabrikasını başlatın.
	DX::ThrowIfFailed(
		CoCreateInstance(
			CLSID_WICImagingFactory2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_wicFactory)
			)
		);
}

// Direct3D cihazını yapılandırır ve ilgili tanıtıcıları ve cihaz bağlamını depolar.
void DX::DeviceResources::CreateDeviceResources() 
{
	// Bu bayrak API varsayılanından farklı renk kanalı sıralaması olan yüzeylere yönelik
	// destek ekler. Direct2D uyumluluğu için gereklidir.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (DX::SdkLayersAvailable())
	{
		// Proje hata ayıklama yapısındaysa SDK Katmanları üzerinden hata ayıklamayı bu bayrakla etkinleştirin.
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	// Bu dizi, bu uygulamanın destekleyeceği DirectX donanım özelliği düzeyleri kümesini tanımlar.
	// Sıralama korunmalıdır.
	// Uygulamanıza gereken en düşük özellik düzeyini açıklaması içinde belirtmeyi
	// unutmayın. Başka türlü belirtilmedikçe tüm uygulamaların 9.1'i desteklediği varsayılır.
	D3D_FEATURE_LEVEL featureLevels[] = 
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	// Direct3D 11 API'si cihaz nesnesini ve karşılık gelen bağlamı oluşturun.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	HRESULT hr = D3D11CreateDevice(
		nullptr,					// Varsayılan bağdaştırıcıyı kullanmak için nullptr belirtin.
		D3D_DRIVER_TYPE_HARDWARE,	// Donanım grafik sürücüsünü kullanan bir cihaz oluşturun.
		0,							// Sürücü D3D_DRIVER_TYPE_SOFTWARE olmadığı sürece 0 olmalıdır.
		creationFlags,				// Hata ayıklama ve Direct2D uyumluluk bayraklarını ayarlayın.
		featureLevels,				// Bu uygulamanın destekleyebileceği özellik düzeylerini listeleyin.
		ARRAYSIZE(featureLevels),	// Yukarıdaki listenin boyutu.
		D3D11_SDK_VERSION,			// Microsoft Store uygulamaları için bunu her zaman D3D11_SDK_VERSION olarak ayarlayın.
		&device,					// Oluşturulan Direct3D cihazını döndürür.
		&m_d3dFeatureLevel,			// Cihazın oluşturulan özellik düzeyini döndürür.
		&context					// Cihazın anlık bağlamını döndürür.
		);

	if (FAILED(hr))
	{
		// Başlatma başarısız olursa WARP cihazına geri dönün.
		// WARP hakkında daha fazla bilgi için, bkz.: 
		// https://go.microsoft.com/fwlink/?LinkId=286690
		DX::ThrowIfFailed(
			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP, // Donanım cihazı yerine bir WARP cihazı oluşturun.
				0,
				creationFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				&device,
				&m_d3dFeatureLevel,
				&context
				)
			);
	}

	// Direct3D 11.3 API'si cihazına ve anlık bağlama yönelik işaretçileri depolayın.
	DX::ThrowIfFailed(
		device.As(&m_d3dDevice)
		);

	DX::ThrowIfFailed(
		context.As(&m_d3dContext)
		);

	// Direct2D cihazı nesnesini ve karşılık gelen bağlamı oluşturun.
	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(
		m_d3dDevice.As(&dxgiDevice)
		);

	DX::ThrowIfFailed(
		m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice)
		);

	DX::ThrowIfFailed(
		m_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			&m_d2dContext
			)
		);
}

// Bu kaynakların pencere boyutunun her değiştirilişinde yeniden oluşturulması gerekir.
void DX::DeviceResources::CreateWindowSizeDependentResources() 
{
	// Önceki pencere boyutuna özgü içeriği temizleyin.
	ID3D11RenderTargetView* nullViews[] = {nullptr};
	m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
	m_d3dRenderTargetView = nullptr;
	m_d2dContext->SetTarget(nullptr);
	m_d2dTargetBitmap = nullptr;
	m_d3dDepthStencilView = nullptr;
	m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

	UpdateRenderTargetSize();

	// Takas zincirinin genişliği ve uzunluğu, pencerenin yerel yönelimli
	// genişliğine ve uzunluğuna bağlıdır. Pencere yerel yönlendirmede
	// değilse boyutların tersine çevrilmesi gerekir.
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	if (m_swapChain != nullptr)
	{
		// Takas zinciri zaten mevcutsa zinciri yeniden boyutlandırın.
		HRESULT hr = m_swapChain->ResizeBuffers(
			2, // İki kez arabelleğe alınmış takas zinciri.
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// Cihaz herhangi bir nedenle kaldırılmışsa, yeni bir cihazın ve takas zincirinin oluşturulması gerekir.
			HandleDeviceLost();

			// Tüm kurulumlar gerçekleştirildi. Bu yöntemi yürütmeye devam etmeyin. HandleDeviceLost, bu yöntemi 
			// yeniden girecek ve yeni cihazın kurulumunu yapacak.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// Aksi durumda mevcut Direct3D cihazının kullandığı bağdaştırıcıyla yenisini oluşturun.
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// Pencerenin boyutunu eşleştirin.
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// Bu en yaygın takas zinciri biçimidir.
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// Çoklu örnekleme kullanmayın.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;									// Gecikme süresini en aza indirmek için iki kez arabelleğe alma kullanın.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// Tüm Microsoft Store uygulamaları _FLIP_ SwapEffects kullanmalıdır.
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// Bu dizi, yukarıdaki Direct3D cihazını oluşturmak için kullanılan DXGI fabrikasını alır.
		ComPtr<IDXGIDevice3> dxgiDevice;
		DX::ThrowIfFailed(
			m_d3dDevice.As(&dxgiDevice)
			);

		ComPtr<IDXGIAdapter> dxgiAdapter;
		DX::ThrowIfFailed(
			dxgiDevice->GetAdapter(&dxgiAdapter)
			);

		ComPtr<IDXGIFactory4> dxgiFactory;
		DX::ThrowIfFailed(
			dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
			);

		// XAML birlikte çalışması kullanılırken oluşturma için takas zinciri oluşturulmalıdır.
		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			dxgiFactory->CreateSwapChainForComposition(
				m_d3dDevice.Get(),
				&swapChainDesc,
				nullptr,
				&swapChain
				)
			);

		DX::ThrowIfFailed(
			swapChain.As(&m_swapChain)
			);

		// Takas zincirini SwapChainPanel ile ilişkilendirin
		// UI değişikliklerinin UI iş parçacığına geri gönderilmesi gerekir
		m_swapChainPanel->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([=]()
		{
			// SwapChainPanel için yedekleme yerel arabirimi alın
			ComPtr<ISwapChainPanelNative> panelNative;
			DX::ThrowIfFailed(
				reinterpret_cast<IUnknown*>(m_swapChainPanel)->QueryInterface(IID_PPV_ARGS(&panelNative))
				);

			DX::ThrowIfFailed(
				panelNative->SetSwapChain(m_swapChain.Get())
				);
		}, CallbackContext::Any));

		// DXGI'nin tek seferde birden fazla kareyi kuyruğa almadığından emin olun. Bu hem gecikme süresini kısaltır,
		// hem de uygulamanın güç tüketimini en aza indirerek yalnızca her bir VSync sonrasında işlemesini sağlar.
		DX::ThrowIfFailed(
			dxgiDevice->SetMaximumFrameLatency(1)
			);
	}

	// Takas zinciri için doğru yönlendirmeyi ayarlayın ve döndürülmüş
	// takas zincirine işlemek için 2D ve 3D matris dönüştürmeleri oluşturun.
	// 2D ve 3D dönüşümlerinin döndürme açılarının farklı olduğunu unutmayın.
	// Bu durum, koordinat alanlarındaki farklılıktan kaynaklanır. Aynı zamanda,
	// yuvarlama hatalarından kaçınmak için 3D matrisi açıkça belirtilmiştir.

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform2D = Matrix3x2F::Identity();
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(90.0f) *
			Matrix3x2F::Translation(m_logicalSize.Height, 0.0f);
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(180.0f) *
			Matrix3x2F::Translation(m_logicalSize.Width, m_logicalSize.Height);
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(270.0f) *
			Matrix3x2F::Translation(0.0f, m_logicalSize.Width);
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
		);

	// Takas zinciri üzerinde ters ölçek oluşturun
	DXGI_MATRIX_3X2_F inverseScale = { 0 };
	inverseScale._11 = 1.0f / m_effectiveCompositionScaleX;
	inverseScale._22 = 1.0f / m_effectiveCompositionScaleY;
	ComPtr<IDXGISwapChain2> spSwapChain2;
	DX::ThrowIfFailed(
		m_swapChain.As<IDXGISwapChain2>(&spSwapChain2)
		);

	DX::ThrowIfFailed(
		spSwapChain2->SetMatrixTransform(&inverseScale)
		);

	// Takas zinciri arka arabelleğinin işlem hedef görünümü oluşturun.
	ComPtr<ID3D11Texture2D1> backBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
		);

	DX::ThrowIfFailed(
		m_d3dDevice->CreateRenderTargetView1(
			backBuffer.Get(),
			nullptr,
			&m_d3dRenderTargetView
			)
		);

	// Gerektiğinde 3D oluşturma ile kullanmak için bir derinlik kalıbı görünümü oluşturun.
	CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
		DXGI_FORMAT_D24_UNORM_S8_UINT, 
		lround(m_d3dRenderTargetSize.Width),
		lround(m_d3dRenderTargetSize.Height),
		1, // Bu derinlik kalıbı görünümünde yalnızca bir doku bulunur.
		1, // Tek bir mipmap düzeyi kullanın.
		D3D11_BIND_DEPTH_STENCIL
		);

	ComPtr<ID3D11Texture2D1> depthStencil;
	DX::ThrowIfFailed(
		m_d3dDevice->CreateTexture2D1(
			&depthStencilDesc,
			nullptr,
			&depthStencil
			)
		);

	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
	DX::ThrowIfFailed(
		m_d3dDevice->CreateDepthStencilView(
			depthStencil.Get(),
			&depthStencilViewDesc,
			&m_d3dDepthStencilView
			)
		);

	// 3D işleme görünüm penceresini, pencerenin tamamını hedefleyecek şekilde ayarlayın.
	m_screenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		m_d3dRenderTargetSize.Width,
		m_d3dRenderTargetSize.Height
		);

	m_d3dContext->RSSetViewports(1, &m_screenViewport);

	// İşleme hedefiyle ilişkilendirilen bir Direct2D hedef bit eşlemi
	// zincirin geri arabelleğini değiştirin ve geçerli hedef olarak ayarlayın.
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = 
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			m_dpi,
			m_dpi
			);

	ComPtr<IDXGISurface2> dxgiBackBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
		);

	DX::ThrowIfFailed(
		m_d2dContext->CreateBitmapFromDxgiSurface(
			dxgiBackBuffer.Get(),
			&bitmapProperties,
			&m_d2dTargetBitmap
			)
		);

	m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
	m_d2dContext->SetDpi(m_effectiveDpi, m_effectiveDpi);

	// Tüm Microsoft Store uygulamaları için gri tonlamalı metin düzgünleştirme önerilir.
	m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

// Oluşturma hedefinin boyutlarını ve ölçeğinin azaltılıp azaltılmayacağını belirleyin.
void DX::DeviceResources::UpdateRenderTargetSize()
{
	m_effectiveDpi = m_dpi;
	m_effectiveCompositionScaleX = m_compositionScaleX;
	m_effectiveCompositionScaleY = m_compositionScaleY;

	// Yüksek çözünürlüklü cihazlarda pil ömrünü artırmak için daha küçük bir işleme hedefinde işleyin
	// ve GPU'nun sunulan çıkışı ölçeklendirmesine izin verin.
	if (!DisplayMetrics::SupportHighResolutions && m_dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
		float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

		// Cihaz dikey yöndeyken yükseklik genişlikten büyüktür. Büyük boyut
		// ile genişlik eşiğini ve küçük boyut ile yükseklik eşiğini
		// karşılaştırın.
		if (max(width, height) > DisplayMetrics::WidthThreshold && min(width, height) > DisplayMetrics::HeightThreshold)
		{
			// Uygulamayı ölçeklendirmek için etkin DPI değiştirilir. Mantıksal boyut değişmez.
			m_effectiveDpi /= 2.0f;
			m_effectiveCompositionScaleX /= 2.0f;
			m_effectiveCompositionScaleY /= 2.0f;
		}
	}

	// Gerekli oluşturma hedef boyutunu piksel cinsinden hesaplayın.
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// Boyutu sıfır olan DirectX içeriğinin oluşturulmasını önleyin.
	m_outputSize.Width = max(m_outputSize.Width, 1);
	m_outputSize.Height = max(m_outputSize.Height, 1);
}

// Bu yöntem, XAML denetimi oluşturulduğunda (veya yeniden oluşturulduğunda) çağrılır.
void DX::DeviceResources::SetSwapChainPanel(SwapChainPanel^ panel)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_swapChainPanel = panel;
	m_logicalSize = Windows::Foundation::Size(static_cast<float>(panel->ActualWidth), static_cast<float>(panel->ActualHeight));
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	m_compositionScaleX = panel->CompositionScaleX;
	m_compositionScaleY = panel->CompositionScaleY;
	m_dpi = currentDisplayInformation->LogicalDpi;
	m_d2dContext->SetDpi(m_dpi, m_dpi);

	CreateWindowSizeDependentResources();
}

// Bu yöntem, SizeChanged etkinliği için etkinlik işleyicisinde çağırılır.
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// Bu yöntem, DpiChanged olayı için olay işleyicisinde çağrılır.
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;
		m_d2dContext->SetDpi(m_dpi, m_dpi);
		CreateWindowSizeDependentResources();
	}
}

// Bu yöntem, OrientationChanged olayı için olay işleyicisinde çağrılır.
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// Bu yöntem, CompositionScaleChanged olayı için olay işleyicisinde çağrılır.
void DX::DeviceResources::SetCompositionScale(float compositionScaleX, float compositionScaleY)
{
	if (m_compositionScaleX != compositionScaleX ||
		m_compositionScaleY != compositionScaleY)
	{
		m_compositionScaleX = compositionScaleX;
		m_compositionScaleY = compositionScaleY;
		CreateWindowSizeDependentResources();
	}
}

// Bu yöntem, DisplayContentsInvalidated olayı için olay işleyicisinde çağrılır.
void DX::DeviceResources::ValidateDevice()
{
	// D3D Cihazı, cihaz oluşturulduktan sonra varsayılan bağdaştırıcı değiştiyse veya
	// cihaz kaldırıldıysa artık geçerli değildir.

	// Öncelikle cihaz oluşturulduğu sıradaki varsayılan bağdaştırıcı hakkında bilgi alın.

	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

	ComPtr<IDXGIAdapter> deviceAdapter;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

	ComPtr<IDXGIFactory2> deviceFactory;
	DX::ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	ComPtr<IDXGIAdapter1> previousDefaultAdapter;
	DX::ThrowIfFailed(deviceFactory->EnumAdapters1(0, &previousDefaultAdapter));

	DXGI_ADAPTER_DESC1 previousDesc;
	DX::ThrowIfFailed(previousDefaultAdapter->GetDesc1(&previousDesc));

	// Sonra geçerli varsayılan bağdaştırıcı hakkında bilgi alın.

	ComPtr<IDXGIFactory4> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC1 currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc1(&currentDesc));

	// Bağdaştırıcı LUID'leri eşleşmiyorsa veya cihaz kaldırıldığını bildiriyorsa,
	// yeni bir D3D cihazı oluşturulması gerekir.

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		// Eski cihazla ilgili kaynaklara yönelik başvuruları bırakın.
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		// Yeni bir cihaz ve takas zinciri oluşturun.
		HandleDeviceLost();
	}
}

// Tüm cihaz kaynaklarını yeniden oluşturun ve bunları tekrar geçerli duruma ayarlayın.
void DX::DeviceResources::HandleDeviceLost()
{
	m_swapChain = nullptr;

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

	CreateDeviceResources();
	m_d2dContext->SetDpi(m_dpi, m_dpi);
	CreateWindowSizeDependentResources();

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
}

// Cihazın kaybolması ve oluşturulması durumunda bilgilendirilmek için DeviceNotify işlevimize kaydolun.
void DX::DeviceResources::RegisterDeviceNotify(DX::IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

// Uygulama askıda kaldığında bu yöntemi çağırın. Sürücüye uygulamanın boşta durumuna girdiğini 
// ve geçici arabelleklerin diğer uygulamalar tarafından kullanılmak üzere geri kazanılabileceğini belirten bir ipucu sağlar.
void DX::DeviceResources::Trim()
{
	ComPtr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);

	dxgiDevice->Trim();
}

// Takas zincirinin içeriklerini ekrana sunun.
void DX::DeviceResources::Present() 
{
	// İlk bağımsız değişken DXGI'yi VSync'e kadar engellemeye yönlendirir, uygulamaları bir sonraki
	// VSync'e kadar uyutur. Bu, ekranda asla görüntülenmeyecek çerçeveler işleyen hiçbir
	// döngüyü harcamamamızı sağlar.
	DXGI_PRESENT_PARAMETERS parameters = { 0 };
	HRESULT hr = m_swapChain->Present1(1, 0, &parameters);

	// İşleme hedefinin içeriğini atın.
	// Bu yalnızca var olan içeriğin tamamen üzerine yazıldığında geçerli bir
	// üzerine yazıldı. Değiştirilmiş dikdörtgenler veya kaydırma dikdörtgenleri kullanılıyorsa bu çağrı değiştirilmelidir.
	m_d3dContext->DiscardView1(m_d3dRenderTargetView.Get(), nullptr, 0);

	// Derinlik kalıbının içeriğini atın.
	m_d3dContext->DiscardView1(m_d3dDepthStencilView.Get(), nullptr, 0);

	// Bağlantı kesilmesi veya sürücü yükseltmesi nedeniyle cihaz kaldırıldıysa 
	// tüm cihaz kaynaklarını yeniden oluşturmalıyız.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HandleDeviceLost();
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}
}

// Bu metot, görüntü cihazının yerel yönü ile geçerli görüntü yönü arasındaki
// döndürmeyi belirler.
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// Not: DisplayOrientations numaralandırması farklı değerlere sahip olsa bile
	// NativeOrientation yalnızca Yatay veya Dikey olabilir.
	switch (m_nativeOrientation)
	{
	case DisplayOrientations::Landscape:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;
		}
		break;

	case DisplayOrientations::Portrait:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;
		}
		break;
	}
	return rotation;
}