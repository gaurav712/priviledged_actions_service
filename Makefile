CC="gcc"

priviledged_actions_service: priviledged_actions_service.c
	$(CC) -o priviledged_actions_service priviledged_actions_service.c

install: priviledged_actions_service
	sudo cp ./priviledged_actions_service /usr/local/bin/priviledged_actions_service

clean:
	rm -f ./priviledged_actions_service
