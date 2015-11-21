#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/time.h>
#include <assert.h>

#define POLL_HZ 50

#define K_CTRL KEY_LEFTCTRL
#define K_ALT  KEY_LEFTALT
#define K_WIN  KEY_LEFTMETA

#define S1 TIOCM_CD
#define S2 TIOCM_RI

#define make_key(k) \
static void k ##_down() \
{ \
	system("xsetroot -name '" #k " Mode On'"); \
	sendkey(input_fd, K_##k, 1); \
} \
\
static void k ##_up() \
{ \
	system("xsetroot -name '" #k " Mode Off'"); \
	sendkey(input_fd, K_##k, 0); \
}


int is_event_device(const struct dirent *dir)
{
	return strncmp("event", dir->d_name, 5) == 0;
}

int open_input(char *name)
{
	int i;
	int fd, ndev;
	struct dirent **namelist;
	char xname[256]= "Unknown";
	char fname[256];
	ndev = scandir("/dev/input", &namelist, is_event_device, versionsort);
	if(ndev < 0) {
		perror("scandir");
		abort();
	}
	for(i = 0; i < ndev; i++) {
		snprintf(fname, sizeof(fname), "/dev/input/%s", namelist[i]->d_name);
		fd = open(fname, O_RDONLY);
		if(fd < 0) {
			perror("open");
			abort();
		}
		ioctl(fd, EVIOCGNAME(sizeof(xname)), xname);
		printf("name : %s\n", xname);
		if(strstr(xname, name)) {
			int j;
			for(j = 0; j < ndev; j++)
				free(namelist[j]);
			free(namelist);
			return fd;
		}
		close(fd);
	}
	abort();
}

int serial_open()
{
	int fd;
	int status;
	fd = open("/dev/ttyUSB0", O_RDONLY);
	status = TIOCM_DTR;
	ioctl(fd, TIOCMBIS, &status);
	status = TIOCM_RTS;
	ioctl(fd, TIOCMBIC, &status);

	ioctl(fd, TIOCMGET, &status);
	assert(status & TIOCM_DTR);
	assert(!(status & TIOCM_RTS));
	return fd;
}

int serial_get_status(int fd)
{
	int status;
	ioctl(fd, TIOCMGET, &status);
	return status;
}

int uinput_open()
{
	int i;
	int uifd;
	struct uinput_user_dev uinp;
	uifd = open("/dev/uinput", O_WRONLY | O_NDELAY);
	if(uifd == 0) {
		perror("uinput");
		abort();
	}

	memset(&uinp, 0, sizeof(uinp));
	strcpy(uinp.name, "virtual dev");
	uinp.id.version = 4;
	uinp.id.bustype = BUS_USB;

	ioctl(uifd, UI_SET_EVBIT, EV_KEY);
	ioctl(uifd, UI_SET_EVBIT, EV_REL);
	ioctl(uifd, UI_SET_RELBIT, REL_X);
	ioctl(uifd, UI_SET_RELBIT, REL_Y);
	ioctl(uifd, UI_SET_RELBIT, REL_WHEEL);
	for(i = 0; i < 256; i++) {
		ioctl(uifd, UI_SET_KEYBIT, i);
	}
	ioctl(uifd, UI_SET_KEYBIT, BTN_MOUSE);
	ioctl(uifd, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(uifd, UI_SET_KEYBIT, BTN_MIDDLE);
	ioctl(uifd, UI_SET_KEYBIT, BTN_RIGHT);
	ioctl(uifd, UI_SET_KEYBIT, BTN_FORWARD);
	ioctl(uifd, UI_SET_KEYBIT, BTN_BACK);

	write(uifd, &uinp, sizeof(uinp));
	if(ioctl(uifd, UI_DEV_CREATE)) {
		perror("uinput create");
		abort();
	}

	return uifd;
}

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

void move_wheel(int fd, int x, int y)
{
	struct input_event ev;
	memset(&ev, 0, sizeof(ev));
	if(y) {
		ev.type = EV_REL;
		ev.code = REL_WHEEL;
		ev.value = -y;
		gettimeofday(&ev.time, 0);
		write(fd, &ev, sizeof(ev));
	}

	if(x || y) {
		memset(&ev, 0, sizeof(ev));
		ev.type = EV_SYN;
		ev.code = SYN_REPORT;
		ev.value = 0;
		gettimeofday(&ev.time, 0);
		write(fd, &ev, sizeof(ev));
	}
}

void move_xy(int fd, int x, int y)
{
	struct input_event ev;
	memset(&ev, 0, sizeof(ev));
	if(x) {
		ev.type = EV_REL;
		ev.code = REL_X;
		ev.value = x;
		gettimeofday(&ev.time, 0);
		write(fd, &ev, sizeof(ev));
	}

	if(y) {
		ev.type = EV_REL;
		ev.code = REL_Y;
		ev.value = y;
		gettimeofday(&ev.time, 0);
		write(fd, &ev, sizeof(ev));
	}

	if(x || y) {
		memset(&ev, 0, sizeof(ev));
		ev.type = EV_SYN;
		ev.code = SYN_REPORT;
		ev.value = 0;
		gettimeofday(&ev.time, 0);
		write(fd, &ev, sizeof(ev));
	}
}

void release_keys(int fd, int input_fd)
{
	int i, j;
	unsigned char keys[KEY_MAX/8 + 1];
	memset(keys, 0, sizeof keys);

	ioctl(fd, EVIOCGKEY(sizeof keys), keys);

	for(i = 0; i < sizeof keys; i++) {
		for(j = 0; j < 8; j++) {
			if(keys[i] & (1 << j)) {
				sendkey(input_fd, i*8 + j, 0);
			}
		}
	}
}

struct fs_state {
	int state;
	void *priv;
	void (*click_down1)(struct fs_state *);
	void (*click_up1)(struct fs_state *);
	void (*click_down2)(struct fs_state *);
	void (*click_up2)(struct fs_state *);
	void (*click_down3)(struct fs_state *);
	void (*click_up3)(struct fs_state *);
	int last_signal;
	struct timeval last_tv, last2_tv, tv;
};

void nop(struct fs_state *fs)
{
	printf("fs=%p, state=%d\n", fs, fs->state);
}

void fs_run(struct fs_state *fs, int signal)
{
	struct timeval res;
	if(signal && fs->last_signal) {
		if(fs->state == 0) {
			gettimeofday(&fs->tv, NULL);
			timersub(&fs->tv, &fs->last_tv, &res);

			if(res.tv_sec == 0 && res.tv_usec < 500000) {
				timersub(&fs->last_tv, &fs->last2_tv, &res);

				if(res.tv_sec == 0 && res.tv_usec < 500000) {
					fs->state = 3;
					fs->click_down3(fs);
				} else {
					fs->state = 2;
					fs->click_down2(fs);
				}
			} else {
				fs->state = 1;
				fs->click_down1(fs);
			}
			fs->last2_tv = fs->last_tv;
			fs->last_tv = fs->tv;
		}
	}

	if(!signal && !fs->last_signal) {
		int state = fs->state;
		fs->state = 0;
		if(state == 1) {
			fs->click_up1(fs);
		} else if(state == 2) {
			fs->click_up2(fs);
		} else if(state == 3) {
			fs->click_up3(fs);
		}
	}
	fs->last_signal = signal;
}

int input_fd, kfd;

make_key(CTRL)
make_key(ALT)
make_key(WIN)

struct vmouse_state {
	int mouse_mode;
	int mx, my, mc;
};

static void vmouse_down(struct vmouse_state *ms)
{
	system("xsetroot -name 'Mouse Mode On'");
	release_keys(kfd, input_fd);
	ms->mx = 0;
	ms->my = 0;
	ms->mc = 0;
	ms->mouse_mode = 1;
}

static void vmouse_up(struct vmouse_state *ms)
{
	system("xsetroot -name 'Mouse Mode Off'");
	ms->mouse_mode = 0;
}

static void vmouse_key(struct vmouse_state *ms, struct input_event *ev)
{
	if(ev->type == EV_KEY) {
		switch(ev->code) {
		case KEY_LEFTCTRL:
		case KEY_RIGHTCTRL:
		case KEY_LEFTALT:
		case KEY_RIGHTALT:
		case KEY_LEFTMETA:
		case KEY_RIGHTMETA:
		case KEY_LEFTSHIFT:
		case KEY_RIGHTSHIFT:
			sendkey(input_fd, ev->code, ev->value);
			break;
		case KEY_S:
			if(ev->value == 1) {
				ms->mx = -8;
			} else if(ev->value == 0) {
				ms->mx = 0;
			}
			break;
		case KEY_D:
			if(ev->value == 1) {
				ms->my = 8;
			} else if(ev->value == 0) {
				ms->my = 0;
			}
			break;
		case KEY_F:
			if(ev->value == 1) {
				ms->mx = 8;
			} else if(ev->value == 0) {
				ms->mx = 0;
			}
			break;
		case KEY_E:
			if(ev->value == 1) {
				ms->my = -8;
			} else if(ev->value == 0) {
				ms->my = 0;
			}
			break;
		case KEY_J:
			if(ev->value == 1) {
				sendkey(input_fd, BTN_LEFT, 1);
			} else if(ev->value == 0) {
				sendkey(input_fd, BTN_LEFT, 0);
			}
			break;
		case KEY_K:
			if(ev->value == 1) {
				sendkey(input_fd, BTN_MIDDLE, 1);
			} else if(ev->value == 0) {
				sendkey(input_fd, BTN_MIDDLE, 0);
			}
			break;
		case KEY_L:
			if(ev->value == 1) {
				sendkey(input_fd, BTN_RIGHT, 1);
			} else if(ev->value == 0) {
				sendkey(input_fd, BTN_RIGHT, 0);
			}
			break;
		case KEY_SPACE:
			if(ev->value == 1) {
				ms->mc = 1;
			} else if(ev->value == 0) {
				ms->mc = 0;
			}
			break;
		}
	}
}

void vmouse_run(struct vmouse_state *ms)
{
	if(ms->mouse_mode) {
		if(ms->mc) {
			move_wheel(input_fd, ms->mx/24, ms->my/24);
		} else {
			move_xy(input_fd, ms->mx/8, ms->my/8);
		}
		if(ms->mx < 0)
			ms->mx = ms->mx*7/8 - 24;
		if(ms->mx > 0)
			ms->mx = ms->mx*7/8 + 24;
		if(ms->my < 0)
			ms->my = ms->my*7/8 - 24;
		if(ms->my > 0)
			ms->my = ms->my*7/8 + 24;
	}
}

struct fs_priv {
	struct vmouse_state ms;
	struct fs_state *fs1;
	struct fs_state *fs2;
	int meta_down;  /* 0: none, 1: win, 2: alt */
};

static void META_down(struct fs_priv *fsp, int d)
{
	fsp->meta_down = d;
	switch(d) {
	case 1:
		WIN_down();
		break;
	case 2:
		ALT_down();
		break;
	}
}

static void META_up(struct fs_priv *fsp)
{
	int d = fsp->meta_down;
	fsp->meta_down = 0;
	switch(d) {
	case 1:
		WIN_up();
		break;
	case 2:
		ALT_up();
		break;
	}
}

static void fs1_down1(struct fs_state *fs)
{
	struct fs_priv *fsp = fs->priv;
	if(fsp->fs2->state == 1) {
		fsp->fs2->click_up1(fsp->fs2);
		META_down(fsp, 2);
	} else {
		fsp->meta_down = 0;
		vmouse_down(&fsp->ms);
	}
}

static void fs1_down2(struct fs_state *fs)
{
	struct fs_priv *fsp = fs->priv;
	if(fsp->fs2->state == 1) {
		META_down(fsp, 2);
	} else {
		nop(fs);
	}
}

static void fs1_up1(struct fs_state *fs)
{
	struct fs_priv *fsp = fs->priv;
	if(fsp->meta_down) {
		META_up(fsp);
		if(fsp->fs2->state == 1)
			fsp->fs2->click_down1(fsp->fs2);
	} else {
		vmouse_up(&fsp->ms);
	}
}

static void fs1_up2(struct fs_state *fs)
{
	struct fs_priv *fsp = fs->priv;
	if(fsp->meta_down) {
		META_up(fsp);
	} else {
		nop(fs);
	}
}

static void fs2_down1(struct fs_state *fs)
{
	struct fs_priv *fsp = fs->priv;
	if(fsp->fs1->state == 1) {
		fsp->fs1->click_up1(fsp->fs1);
		META_down(fsp, 1);
	} else {
		fsp->meta_down = 0;
		CTRL_down();
	}
}


static void fs2_up1(struct fs_state *fs)
{
	struct fs_priv *fsp = fs->priv;
	if(fsp->meta_down) {
		META_up(fsp);
		if(fsp->fs1->state == 1)
			fsp->fs1->click_down1(fsp->fs1);
	} else {
		CTRL_up();
	}
}

int main(int argc, char *argv[])
{
	int fd;
	struct fs_priv *fsp = malloc(sizeof(struct fs_priv));

	struct fs_state fs1 = {
		.click_down1 = fs1_down1,
		.click_down2 = fs1_down2,
		.click_down3 = nop,
		.click_up1 = fs1_up1,
		.click_up2 = fs1_up2,
		.click_up3 = nop,
		.priv = fsp,
	};

	struct fs_state fs2 = {
		.click_down1 = fs2_down1,
		.click_down2 = nop,
		.click_down3 = nop,
		.click_up1 = fs2_up1,
		.click_up2 = nop,
		.click_up3 = nop,
		.priv = fsp,
	};

	struct vmouse_state *ms = &fsp->ms;
	ms->mouse_mode = 0;
	fsp->fs1 = &fs1;
	fsp->fs2 = &fs2;

	fd = serial_open();
	input_fd = uinput_open();
	kfd = open_input("CATEX TECH. 104EC-Pro");

	printf("wait...\n");
	sleep(1);
	printf("begin...\n");
	ioctl(kfd, EVIOCGRAB, 1);

	for(;;) {
		int status = serial_get_status(fd);
		fd_set rfds;
		struct timeval timeout = {
			.tv_sec = 0,
			.tv_usec = 1000000 / POLL_HZ
		};

		FD_ZERO(&rfds);
		FD_SET(kfd, &rfds);
		select(kfd + 1, &rfds, NULL, NULL, &timeout);

		if(FD_ISSET(kfd, &rfds)) {
			struct input_event ev;
			if(read(kfd, &ev, sizeof(ev)) < 0) {
				perror("read");
				abort();
			}

			if(ms->mouse_mode) {
				vmouse_key(ms, &ev);
			} else {
				if(ev.type == EV_KEY && ev.code == KEY_CAPSLOCK)
					sendkey(input_fd, K_WIN, ev.value);
				else
					write(input_fd, &ev, sizeof(ev));
			}
		} else {
			vmouse_run(ms);
		}

		fs_run(&fs1, status & S1);
		fs_run(&fs2, status & S2);
	}

	return 0;
}
