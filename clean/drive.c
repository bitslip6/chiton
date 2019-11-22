

/**
 * blocks and turns a random direction with no leg or wall
 */
void random_direction() {

	// pick a random direction to turn
	//TODO:del int direction = rand_r(&seed) % 2;
	if (rand_r(&seed) % 2 == 1) {
		Left();
	} else {
		Right();
	}
	// turn for a bit...
	int min_turn_time = 700000 + ((rand_r(&seed) % 5) * 600000);
	usleep(min_turn_time);

	// keep turning until no wall or leg is found...
	int max_wait = 400;
	while(condition == 'W' || condition == 'L' && max_wait > 0) {
		usleep(20000);
		max_wait--;
	}

    // Walls everywhere!
	if (max_wait <= 0) {
        sprintf(text_buff, "unable to move");
		sleep(5);
        return;
	}

    // ahh, nothinng in front
    // turn just a bit more to get that out of the way
	usleep(800000);

	// stop and return
	AllStop();
}



/**
 * drive arround randomly and dont bump into things forever
void *random_drive(void *foo) {
    close(pipe_fd[1]);
    borg_init();

    sprintf(text_buff, "camera wait");
    while(condition == 'W') {
    	usleep(200000);
    }
 */
void *random_drive(void *foo) {
    close(pipe_fd[1]);
    borg_init();

    sprintf(text_buff, "camera wait");
    while(condition == 'W') {
    	usleep(200000);
    }

    // wait for button press
	sprintf(text_buff, (char  *)"press button");
	while(1) {
        if (debounce_read(17) == 0) {
            break;
        }
		usleep(100000);
	}

    // 3 second count down
    for (int i = 3; i != 0; i--) {
	    sprintf(text_buff, (char  *)"Run %d", i);
	    usleep(1000000);
    }

    // loop until running loop ends
    while(running) {
		for(int i=0; i<1000; i++) {
			usleep(10000);

			if (condition == 'W' || condition == 'L') {
				int j = 20;
				AllStop();
				// wait for the thing to get out of the way...
				while (condition == 'W' || condition == 'L' && j > 0) {
				    sprintf(text_buff, (char  *)"wait: %d", j);
					usleep(200000);
					// TODO: DO WE NEED THIS? AllStop();
					j--;
				}

				// it didn't get out of theway...
				if (j <= 0) {
					sprintf(text_buff, (char  *)"Redirect");
					random_direction();
                    // we found a new direction to move in, reset walk counter
					i = 1000;
				}
			}

            sprintf(text_buff, (char *)"Fwd %s", (condition == 'X') ? "fast" : "slow");
            Fwd(condition);
		}
		sprintf(text_buff, (char  *)"Redirect");
		random_direction();
	}

    AllStop();
}


