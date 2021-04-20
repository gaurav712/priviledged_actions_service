CC="gcc"

wlan_toggle_service: wlan_toggle_service.c
	$(CC) -o wlan_toggle_service wlan_toggle_service.c

install: wlan_toggle_service
	sudo cp ./wlan_toggle_service /usr/local/bin/wlan_toggle_service

clean:
	rm -f ./wlan_toggle_service
