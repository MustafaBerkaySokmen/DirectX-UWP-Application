#pragma once

#include <wrl.h>

namespace DX
{
	// Animasyon ve simülasyon zamanlaması için yardımcı sınıfı.
	class StepTimer
	{
	public:
		StepTimer() : 
			m_elapsedTicks(0),
			m_totalTicks(0),
			m_leftOverTicks(0),
			m_frameCount(0),
			m_framesPerSecond(0),
			m_framesThisSecond(0),
			m_qpcSecondCounter(0),
			m_isFixedTimeStep(false),
			m_targetElapsedTicks(TicksPerSecond / 60)
		{
			if (!QueryPerformanceFrequency(&m_qpcFrequency))
			{
				throw ref new Platform::FailureException();
			}

			if (!QueryPerformanceCounter(&m_qpcLastTime))
			{
				throw ref new Platform::FailureException();
			}

			// Maksimum delta değerini saniyenin 1/10'una sıfırlayın.
			m_qpcMaxDelta = m_qpcFrequency.QuadPart / 10;
		}

		// Önceki Güncelleştirme çağrısından bu yana geçen süreyi alın.
		uint64 GetElapsedTicks() const						{ return m_elapsedTicks; }
		double GetElapsedSeconds() const					{ return TicksToSeconds(m_elapsedTicks); }

		// Programın başlatılmasından bu yana geçen toplam süreyi alın.
		uint64 GetTotalTicks() const						{ return m_totalTicks; }
		double GetTotalSeconds() const						{ return TicksToSeconds(m_totalTicks); }

		// Programın başlatılmasından bu yana yapılan güncelleştirmelerin toplam sayısını alın.
		uint32 GetFrameCount() const						{ return m_frameCount; }

		// Geçerli kare hızı değerini alın.
		uint32 GetFramesPerSecond() const					{ return m_framesPerSecond; }

		// Sabit ve değişken zaman adımı modlarından hangisinin kullanılacağını belirleyin.
		void SetFixedTimeStep(bool isFixedTimestep)			{ m_isFixedTimeStep = isFixedTimestep; }

		// Sabit zaman adımı modundayken hangi sıklıkta Güncelleştirme çağırılacağını ayarlayın.
		void SetTargetElapsedTicks(uint64 targetElapsed)	{ m_targetElapsedTicks = targetElapsed; }
		void SetTargetElapsedSeconds(double targetElapsed)	{ m_targetElapsedTicks = SecondsToTicks(targetElapsed); }

		// Tamsayı biçimi süreyi, saniyede 10.000.000 tıklama kullanarak temsil eder.
		static const uint64 TicksPerSecond = 10000000;

		static double TicksToSeconds(uint64 ticks)			{ return static_cast<double>(ticks) / TicksPerSecond; }
		static uint64 SecondsToTicks(double seconds)		{ return static_cast<uint64>(seconds * TicksPerSecond); }

		// Bilerek yapılan bir zamanlama süreksizliğinden sonra (örneğin engelleyici bir GÇ işlemi)
		// sabit zaman adımı mantığının bir yakalama ayarlamasından kaçınmak için bunu çağırın 
		// Çağrıları güncelleştirin.

		void ResetElapsedTime()
		{
			if (!QueryPerformanceCounter(&m_qpcLastTime))
			{
				throw ref new Platform::FailureException();
			}

			m_leftOverTicks = 0;
			m_framesPerSecond = 0;
			m_framesThisSecond = 0;
			m_qpcSecondCounter = 0;
		}

		// Belirtilen Güncelleştirme işlevini uygun sayıda çağırarak zamanlayıcı durumunu güncelleştirin.
		template<typename TUpdate>
		void Tick(const TUpdate& update)
		{
			// Geçerli zamanı sorgulayın.
			LARGE_INTEGER currentTime;

			if (!QueryPerformanceCounter(&currentTime))
			{
				throw ref new Platform::FailureException();
			}

			uint64 timeDelta = currentTime.QuadPart - m_qpcLastTime.QuadPart;

			m_qpcLastTime = currentTime;
			m_qpcSecondCounter += timeDelta;

			// Fazla büyük olan zaman deltalarını sıkıştırın (ör. hata ayıklayıcısında duraklatıldıktan sonra).
			if (timeDelta > m_qpcMaxDelta)
			{
				timeDelta = m_qpcMaxDelta;
			}

			// QPC birimlerini kurallı bir değer biçimine dönüştürün. Bu durum, önceki sıkıştırma nedeniyle taşmaya neden olamaz.
			timeDelta *= TicksPerSecond;
			timeDelta /= m_qpcFrequency.QuadPart;

			uint32 lastFrameCount = m_frameCount;

			if (m_isFixedTimeStep)
			{
				// Sabit zaman adımı güncelleştirme mantığı

				// Uygulama, hedefte geçen süreye (milisaniyenin 1/4'ü) çok yakın çalışıyorsa
				// saati hedeflenen değerle eşleşecek şekilde sıkıştırın. Bu durum, küçük ve ilgili olmayan hataların
				// zaman içinde birikmesini önler. Bu sıkıştırma yapılmazsa, 60 fps sabit güncelleştirme
				// gerektiren, 59.94 NTSC formatlı bir ekranda vsync etkin olarak çalışan bir oyun, bir süre sonra
				// bir kareyi bırakmasına sebep olacak sayıda küçük hata biriktirir. Her şeyin düzgün şekilde 
				// çalışmaya devam etmesi için küçük sapmaların sıfıra yuvarlanması gerekir.

				if (abs(static_cast<int64>(timeDelta - m_targetElapsedTicks)) < TicksPerSecond / 4000)
				{
					timeDelta = m_targetElapsedTicks;
				}

				m_leftOverTicks += timeDelta;

				while (m_leftOverTicks >= m_targetElapsedTicks)
				{
					m_elapsedTicks = m_targetElapsedTicks;
					m_totalTicks += m_targetElapsedTicks;
					m_leftOverTicks -= m_targetElapsedTicks;
					m_frameCount++;

					update();
				}
			}
			else
			{
				// Değişken zaman adımı güncelleştirme mantığı.
				m_elapsedTicks = timeDelta;
				m_totalTicks += timeDelta;
				m_leftOverTicks = 0;
				m_frameCount++;

				update();
			}

			// Geçerli kare hızını izleyin.
			if (m_frameCount != lastFrameCount)
			{
				m_framesThisSecond++;
			}

			if (m_qpcSecondCounter >= static_cast<uint64>(m_qpcFrequency.QuadPart))
			{
				m_framesPerSecond = m_framesThisSecond;
				m_framesThisSecond = 0;
				m_qpcSecondCounter %= m_qpcFrequency.QuadPart;
			}
		}

	private:
		// Kaynak zamanlama verileri QPC birimlerini kullanır.
		LARGE_INTEGER m_qpcFrequency;
		LARGE_INTEGER m_qpcLastTime;
		uint64 m_qpcMaxDelta;

		// Türetilmiş zamanlama verileri kurallı değer biçimi kullanır.
		uint64 m_elapsedTicks;
		uint64 m_totalTicks;
		uint64 m_leftOverTicks;

		// Kare hızını izleyen üyeler.
		uint32 m_frameCount;
		uint32 m_framesPerSecond;
		uint32 m_framesThisSecond;
		uint64 m_qpcSecondCounter;

		// Sabit zaman adımı modunu yapılandıran üyeler.
		bool m_isFixedTimeStep;
		uint64 m_targetElapsedTicks;
	};
}
