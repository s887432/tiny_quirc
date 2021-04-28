/* quirc -- QR-code recognition library
 * Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <quirc.h>
#include <time.h>
#include <getopt.h>
#include "camera.h"
#include "convert.h"
#include "dthash.h"

#include <stdio.h>

/* Collected command-line arguments */
static const char *camera_path = "/dev/video0";
static int video_width = 640;
static int video_height = 480;
static int printer_timeout = 2;

// -----------------------------------------------------------
void show_data(const struct quirc_data *data, struct dthash *dt)
{
	if (dthash_seen(dt, data))
		return;

	printf("==> %s\n", data->payload);

	printf("    Version: %d, ECC: %c, Mask: %d, Type: %d\n\n",
	       data->version, "MLHQ"[data->ecc_level],
	       data->mask, data->data_type);
}
// ---------------------------------------------------------

static int main_loop(struct camera *cam, struct quirc *q)
{
	struct dthash dt;

	dthash_init(&dt, printer_timeout);

	for (;;) {
		int w, h;
		int i, count;
		uint8_t *buf = quirc_begin(q, &w, &h);
		const struct camera_buffer *head;
		const struct camera_parms *parms = camera_get_parms(cam);

		if (camera_dequeue_one(cam) < 0) {
			perror("camera_dequeue_one");
			return -1;
		}

		head = camera_get_head(cam);

		if( parms->format == CAMERA_FORMAT_YUYV )
		{
			yuyv_to_luma(head->addr, w * 2, w, h, buf, w);
		}
		else
		{
			printf("format is not supported\n");
			return -1;
		}

		if (camera_enqueue_all(cam) < 0) {
			perror("camera_enqueue_all");
			return -1;
		}

		quirc_end(q);

		count = quirc_count(q);
		for (i = 0; i < count; i++) {
			struct quirc_code code;
			struct quirc_data data;

			quirc_extract(q, i, &code);
			if (!quirc_decode(&code, &data))
				show_data(&data, &dt);
		}
	}
}

static int run_scanner(void)
{
	struct quirc *qr;
	struct camera cam;
	const struct camera_parms *parms;

	camera_init(&cam);
	if (camera_open(&cam, camera_path, video_width, video_height,
			25, 1) < 0) {
		perror("camera_open");
		goto fail_qr;
	}

	if (camera_map(&cam, 8) < 0) {
		perror("camera_map");
		goto fail_qr;
	}

	if (camera_on(&cam) < 0) {
		perror("camera_on");
		goto fail_qr;
	}

	if (camera_enqueue_all(&cam) < 0) {
		perror("camera_enqueue_all");
		goto fail_qr;
	}

	parms = camera_get_parms(&cam);

	qr = quirc_new();
	if (!qr) {
		perror("couldn't allocate QR decoder");
		goto fail_qr;
	}

	if (quirc_resize(qr, parms->width, parms->height) < 0) {
		perror("couldn't allocate QR buffer");
		goto fail_qr_resize;
	}

	if (main_loop(&cam, qr) < 0)
		goto fail_main_loop;

	quirc_destroy(qr);
	camera_destroy(&cam);

	return 0;

fail_main_loop:
fail_qr_resize:
	quirc_destroy(qr);
fail_qr:
	camera_destroy(&cam);

	return -1;
}

int main(int argc, char **argv)
{
	return run_scanner();
}
