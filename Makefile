
SRCS=vfo.c si570.c sbitx_sound.c fft_filter.c  sbitx_gtk.c sbitx_utils.c \
    i2cbb.c si5351v2.c ini.c hamlib.c queue.c modems.c logbook.c \
		modem_cw.c settings_ui.c oled.c hist_disp.c ntputil.c \
		telnet.c macros.c modem_ft8.c remote.c mongoose.c webserver.c sbitx.c 
OBJS=$(patsubst %.c,%.o, $(SRCS))

LIBS=ft8_lib/libft8.a 
CFLAGS=-g `pkg-config --cflags gtk+-3.0` 
LDFLAGS= -lwiringPi -lasound -lm -lfftw3 -lfftw3f -pthread -lncurses -lsqlite3 \
	`pkg-config --libs gtk+-3.0`

sbitx: $(OBJS)
	gcc $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm $(OBJS) sbitx
