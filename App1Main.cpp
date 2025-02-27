#include "pch.h"
#include "App1Main.h"
#include "Common\DirectXHelper.h"

using namespace App1;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

// Uygulama yüklendiğinde uygulama varlıklarını yükler ve başlatır.
App1Main::App1Main(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources), m_pointerLocationX(0.0f)
{
	// Cihazın kaybolması veya yeniden oluşturulması durumunda bilgi verilmesi için kaydolun
	m_deviceResources->RegisterDeviceNotify(this);

	// TODO: Bunu uygulamanızın içerik başlatmasıyla değiştirin.
	m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(m_deviceResources));

	m_fpsTextRenderer = std::unique_ptr<SampleFpsTextRenderer>(new SampleFpsTextRenderer(m_deviceResources));

	// TODO: Varsayılan değişken zaman adımı modu dışında bir şey istiyorsanız zamanlayıcı ayarlarını değiştirin.
	// örn. 60 FPS sabit zaman adımı güncelleştirme mantığı için şunu çağırın:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

App1Main::~App1Main()
{
	// Cihaz bildiriminin kaydını silin
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Pencere boyutu değiştiğinde uygulama durumunu güncelleştirir (örn. cihaz yönü değişikliği)
void App1Main::CreateWindowSizeDependentResources() 
{
	//TODO: Uygulama içeriğinizin boyuta bağlı olarak başlatmasını bununla değiştirin.
	m_sceneRenderer->CreateWindowSizeDependentResources();
}

void App1Main::StartRenderLoop()
{
	// Animasyon işleme döngüsü zaten çalışıyorsa başka bir iş parçacığı başlatmayın.
	if (m_renderLoopWorker != nullptr && m_renderLoopWorker->Status == AsyncStatus::Started)
	{
		return;
	}

	// Arka plan iş parçacığında çalıştırılacak bir görev oluşturun.
	auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction ^ action)
	{
		// Güncelleştirilen çerçeveyi hesaplayın ve dikey boşluk aralığına göre işleyin.
		while (action->Status == AsyncStatus::Started)
		{
			critical_section::scoped_lock lock(m_criticalSection);
			Update();
			if (Render())
			{
				m_deviceResources->Present();
			}
		}
	});

	// Adanmış bir yüksek öncelikli arka plan iş parçacığında görevi çalıştırın.
	m_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
}

void App1Main::StopRenderLoop()
{
	m_renderLoopWorker->Cancel();
}

// Uygulama durumunu çerçeve başına bir kere güncelleştirir.
void App1Main::Update() 
{
	ProcessInput();

	// Görünüm nesnelerini güncelleştirin.
	m_timer.Tick([&]()
	{
		// TODO: Bunu, uygulamanızın içerik güncelleştirme işlevleriyle değiştirin.
		m_sceneRenderer->Update(m_timer);
		m_fpsTextRenderer->Update(m_timer);
	});
}

// Oyun durumunu güncelleştirmeden önce kullanıcıdan alınan tüm girişi işle
void App1Main::ProcessInput()
{
	// TODO: Buraya çerçeve başına giriş işleme ekleyin.
	m_sceneRenderer->TrackingUpdate(m_pointerLocationX);
}

// Geçerli çerçeveyi geçerli uygulama durumuna göre işler.
// Çerçeve işlendiyse ve görüntülenmeye hazırsa doğru değeri döndürür.
bool App1Main::Render() 
{
	// İlk Güncelleştirmeden önce hiçbir şeyi işlemeye çalışmayın.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	// Görünüm penceresini ekranın tamamını hedefleyecek şekilde sıfırlayın.
	auto viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	// İşleme hedeflerini ekrana göre sıfırlayın.
	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
	context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

	// Geri arabelleği ve derinlik kalıbı görünümünü temizleyin.
	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::CornflowerBlue);
	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Görünüm nesnelerini işleyin.
	// TODO: Bunu, uygulamanızın içerik işleme işlevleriyle değiştirin.
	m_sceneRenderer->Render();
	m_fpsTextRenderer->Render();

	return true;
}

// İşleyicilere cihaz kaynaklarının serbest bırakılması gerektiği bilgisini verir.
void App1Main::OnDeviceLost()
{
	m_sceneRenderer->ReleaseDeviceDependentResources();
	m_fpsTextRenderer->ReleaseDeviceDependentResources();
}

// İşleyicilere cihaz kaynaklarının yeniden oluşturulamayabileceği bilgisini verir.
void App1Main::OnDeviceRestored()
{
	m_sceneRenderer->CreateDeviceDependentResources();
	m_fpsTextRenderer->CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}
