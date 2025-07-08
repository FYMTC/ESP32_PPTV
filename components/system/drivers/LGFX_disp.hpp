#pragma once

#include <LovyanGFX.hpp>
#include "conf.h"


//独自の設定を行うクラスを、LGFX_Deviceから派生して作成します。
class LGFX_tft : public lgfx::LGFX_Device {
    // 接続するパネルの型にあったインスタンスを用意します。
    lgfx::Panel_ST7789     _panel_instance;
    // パネルを接続するバスの種類にあったインスタンスを用意します。
    lgfx::Bus_SPI      _bus_instance;
    // バックライト制御が可能な場合はインスタンスを用意します。(必要なければ削除)
    lgfx::Light_PWM _light_instance;

    public:
        // コンストラクタを作成し、ここで各種設定を行います。
        // クラス名を変更した場合はコンストラクタも同じ名前を指定してください。
        LGFX_tft(void)
        {
            {                                    // バス制御の設定を行います。
      auto cfg = _bus_instance.config(); // バス設定用の構造体を取得します。

      // SPIバスの設定
      cfg.spi_host = SPI3_HOST; // 使用するSPIを選択  ESP32-S2,C3 : SPI2_HOST or SPI3_HOST / ESP32 : VSPI_HOST or HSPI_HOST
      // ※ ESP-IDFバージョンアップに伴い、VSPI_HOST , HSPI_HOSTの記述は非推奨になるため、エラーが出る場合は代わりにSPI2_HOST , SPI3_HOSTを使用してください。
      cfg.spi_mode = cfgspi_mode;                  // 设置SPI通信模式（θ ~ 3）
      cfg.freq_write = 80000000;         // 送信時のSPIクロック (最大80MHz, 80MHzを整数で割った値に丸められます)
      cfg.freq_read = 16000000;          // 受信時のSPIクロック
      cfg.spi_3wire = true;              // 使用MOSI引脚接收时设置true
      cfg.use_lock = true;               // 使用事务锁时设置true  (频繁开关通道，拖慢帧数)
      cfg.dma_channel = SPI_DMA_CH_AUTO; // 使用するDMAチャンネルを設定 (0=DMA不使用 / 1=1ch / 2=ch / SPI_DMA_CH_AUTO=自動設定) S3只能使用自动模式
      // ※ ESP-IDFバージョンアップに伴い、DMAチャンネルはSPI_DMA_CH_AUTO(自動設定)が推奨になりました。1ch,2chの指定は非推奨になります。
      cfg.pin_sclk = disp_sclk; // SPIのSCLKピン番号を設定
      cfg.pin_mosi = disp_mosi; // SPIのMOSIピン番号を設定
      cfg.pin_miso = -1; // SPIのMISOピン番号を設定 (-1 = disable)
      cfg.pin_dc = disp_dc;   // SPIのD/Cピン番号を設定  (-1 = disable)
                         // SDカードと共通のSPIバスを使う場合、MISOは省略せず必ず設定してください。
                         //*/

      _bus_instance.config(cfg);              // 設定値をバスに反映します。
      _panel_instance.setBus(&_bus_instance); // バスをパネルにセットします。
    }

    {                                      // 表示パネル制御の設定を行います。
      auto cfg = _panel_instance.config(); // 表示パネル設定用の構造体を取得します。

      cfg.pin_cs = disp_cs;   // CSが接続されているピン番号   (-1 = disable)
      cfg.pin_rst = disp_rst;  // RSTが接続されているピン番号  (-1 = disable)
      cfg.pin_busy = -1; // BUSYが接続されているピン番号 (-1 = disable)

      // ※ 以下の設定値はパネル毎に一般的な初期値が設定されていますので、不明な項目はコメントアウトして試してみてください。

      cfg.panel_width = DISP_HOR_RES;    // 实际可显示的宽度
      cfg.panel_height = DISP_VER_RES;   // 实际可显示的高度
      cfg.offset_x = 0;         // 面板的X方向偏移量
      cfg.offset_y = disp_offset_y;        // 面板的Y方向偏移量
      cfg.offset_rotation = OFFSET_ROTATION;  // 旋转方向的值偏移0-7(4-7为上下反转 0 竖屏，排线在下 1 横屏，排线在右 2 竖屏，排线在上 3 横屏，排线在左 4 竖屏，排线在上反转 5 横屏，排线在左反转 6 竖屏，排线在下反转 7 横屏，排线在右反转)
      cfg.dummy_read_pixel = 8; // 读取像素数据之前需要进行的无效读取操作的像素数量,消除可能存在的前置数据或杂讯
      cfg.dummy_read_bits = 1;  // 在读取数据时需要额外读取的无效位数量,对齐数据流，或者消除噪声或错误数据
      cfg.readable = cfgreadable;      // 如果可以读取数据，设置为真1/
      cfg.invert = cfginvert;        // 如果面板的明暗反转设置为真
      cfg.rgb_order = cfgrgb_order;     // 如果面板的红蓝互换，设置为真
      cfg.dlen_16bit = cfgdlen_16bit;   // 如果面板使用16bit并行或SPI以16bit为单位发送数据长度，则设置为真
      cfg.bus_shared = cfgbus_shared;    // 如果你和SD卡共享总线，你可以设置为true(通过drawJpgFile等来控制总线)

      _panel_instance.config(cfg);
    }

    //*
    {                                      // バックライト制御の設定を行います。（必要なければ削除）
      auto cfg = _light_instance.config(); // バックライト設定用の構造体を取得します。

      cfg.pin_bl = DISP_bl_pin;     // バックライトが接続されているピン番号
      cfg.invert = false;  // バックライトの輝度を反転させる場合 true
      cfg.freq = 44100;    // バックライトのPWM周波数
      cfg.pwm_channel = 7; // 使用するPWMのチャンネル番号

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance); // バックライトをパネルにセットします。
    }
    //*/

    setPanel(&_panel_instance); // 使用するパネルをセットします。
  }
};
