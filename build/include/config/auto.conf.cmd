deps_config := \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/app_trace/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/aws_iot/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/bt/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/driver/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/esp-mqtt/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/esp32/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/esp_adc_cal/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/esp_http_client/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/ethernet/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/fatfs/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/freertos/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/heap/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/libsodium/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/log/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/lwip/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/mbedtls/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/openssl/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/pthread/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/spi_flash/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/spiffs/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/tcpip_adapter/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/wear_levelling/Kconfig \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/tucker/Desktop/GIT/esp32/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/tucker/Desktop/GIT/esp32/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
