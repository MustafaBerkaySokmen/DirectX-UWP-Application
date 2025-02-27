//
// DirectXPage.xaml.h
// DirectXPage sınıfı bildirimi.
//

#pragma once

#include "DirectXPage.g.h"

#include "Common\DeviceResources.h"
#include "App1Main.h"

namespace App1
{
	/// <summary>
	/// Bir DirectX SwapChainPanel barındıran sayfa.
	/// </summary>
	public ref class DirectXPage sealed
	{
	public:
		DirectXPage();
		virtual ~DirectXPage();

		void SaveInternalState(Windows::Foundation::Collections::IPropertySet^ state);
		void LoadInternalState(Windows::Foundation::Collections::IPropertySet^ state);

	private:
		// XAML düşük-düzey işleme olay işleyicisi.
		void OnRendering(Platform::Object^ sender, Platform::Object^ args);

		// Pencere olay işleyicileri.
		void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);

		// DisplayInformation olay işleyicileri.
		void OnDpiChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnOrientationChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnDisplayContentsInvalidated(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);

		// Diğer olay işleyicileri.
		void AppBarButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnCompositionScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel^ sender, Object^ args);
		void OnSwapChainPanelSizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);

		// Bağımsız girişimizi arka plan çalışan iş parçacığında izleyin.
		Windows::Foundation::IAsyncAction^ m_inputLoopWorker;
		Windows::UI::Core::CoreIndependentInputSource^ m_coreInput;

		// Bağımsız girişin kullanım işlevleri.
		void OnPointerPressed(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerMoved(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerReleased(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);

		// XAML sayfa arka planında DirectX içeriğini işlemek için kullanılan kaynaklar.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<App1Main> m_main; 
		bool m_windowVisible;
	};
}

