#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/time.h>
#include <assert.h>

#define POLL_HZ 50
#define K1 KEY_LEFTCTRL
#define K2 KEY_LEFTMETA
#define K3 KEY_PAGEDOWN

void sendkey(int fd, int code, int value)
{
	struct input_event ev;
	
	memset(&ev, 0, sizeof(ev));
	ev.type = EV_KEY;
	ev.code = code;
	ev.value = value;
	gettimeofday(&ev.time, 0);
	write(fd, &ev, sizeof(ev));

	memset(&ev, 0, sizeof(ev));
	ev.type = EV_SYN;
	ev.code = SYN_REPORT;
	ev.value = 0;
	gettimeofday(&ev.time, 0);
	write(fd, &ev, sizeof(ev));
}

int main(int argc, char *argv[])
{
	int fd;
	int input_fd;
	int status;
	int last_status;
	int state;
	struct timeval last_tv, last2_tv, tv, res;

	fd = open("/dev/ttyUSB0", O_RDONLY);
	input_fd = open("/dev/input/event0", O_RDWR);
	state = 0;
	status = TIOCM_DTR;
	ioctl(fd, TIOCMBIS, &status);
	status = TIOCM_RTS;
	ioctl(fd, TIOCMBIC, &status);

	ioctl(fd, TIOCMGET, &status);
	assert(status & TIOCM_DTR);
	assert(!(status & TIOCM_RTS));

	last_tv.tv_sec = 0;
	last_tv.tv_usec = 0;
	last2_tv.tv_sec = 0;
	last2_tv.tv_usec = 0;

	for(;;) {
		last_status = status;
		ioctl(fd, TIOCMGET, &status);
		usleep(1000000/POLL_HZ);

		if((status & TIOCM_CD) &&
		   (last_status & TIOCM_CD)) {
			if(state == 0) {
				gettimeofday(&tv, NULL);
				timersub(&tv, &last_tv, &res);

				if(res.tv_sec == 0 && res.tv_usec < 500000) {
					timersub(&last_tv, &last2_tv, &res);
					if(res.tv_sec == 0 && res.tv_usec < 500000) {
						sendkey(input_fd, K3, 1);
						state = 3;
					} else {
						sendkey(input_fd, K2, 1);
						state = 2;
					}
				} else {
					sendkey(input_fd, K1, 1);
					state = 1;
				}
				last2_tv = last_tv;
				last_tv = tv;
			}
		}
		if((!(status & TIOCM_CD)) &&
		   (!(last_status & TIOCM_CD))) {
			if(state == 1) {
				sendkey(input_fd, K1, 0);
			} else if(state == 2) {
				sendkey(input_fd, K2, 0);
			} else if(state == 3) {
				sendkey(input_fd, K3, 0);
			}
			state = 0;
		}
	}
	return 0;
}
