// lap_filter2_axis.cpp
// 2017/02/27

#include <stdio.h>
#include <string.h>
#include <ap_int.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

#include "lap_filter_axis.h"

int laplacian_fil(int x0y0, int x1y0, int x2y0, int x0y1, int x1y1, int x2y1, int x0y2, int x1y2, int x2y2);
int conv_rgb2y(int rgb);

int lap_filter_axis(hls::stream<ap_axis<32,1,1,1> >& ins, hls::stream<ap_axis<32,1,1,1> >& outs){
#pragma HLS INTERFACE axis register both port=ins
#pragma HLS INTERFACE axis register both port=outs
#pragma HLS INTERFACE s_axilite port=return

	ap_axis<32,1,1,1> pix;
	ap_axis<32,1,1,1> lap;

	unsigned int line_buf[2][HORIZONTAL_PIXEL_WIDTH];
#pragma HLS array_partition variable=line_buf block factor=2 dim=1
#pragma HLS resource variable=line_buf core=RAM_2P

	int pix_mat[3][3];
#pragma HLS array_partition variable=pix_mat complete

	int lap_fil_val;

	do { // user �� 1�ɂȂ������Ƀt���[�����X�^�[�g����
#pragma HLS LOOP_TRIPCOUNT min=1 max=1 avg=1
		ins >> pix;
	} while(pix.user == 0);

	for (int y=0; y<VERTICAL_PIXEL_WIDTH; y++){
		for (int x=0; x<HORIZONTAL_PIXEL_WIDTH; x++){
#pragma HLS PIPELINE II=1
	        if (!(x==0 && y==0))	// �ŏ��̓��͂͂��łɓ��͂���Ă���
				ins >> pix;	// AXI4-Stream ����̓���

			for (int k=0; k<3; k++){
				for (int m=0; m<2; m++){
					pix_mat[k][m] = pix_mat[k][m+1];
				}
			}
			pix_mat[0][2] = line_buf[0][x];
			pix_mat[1][2] = line_buf[1][x];

			int y_val = conv_rgb2y(pix.data);
			pix_mat[2][2] = y_val;

			line_buf[0][x] = line_buf[1][x];	// �s�̓���ւ�
			line_buf[1][x] = y_val;

			lap_fil_val = laplacian_fil(	pix_mat[0][0], pix_mat[0][1], pix_mat[0][2],
										pix_mat[1][0], pix_mat[1][1], pix_mat[1][2], 
										pix_mat[2][0], pix_mat[2][1], pix_mat[2][2]);
			lap.data = (lap_fil_val<<16)+(lap_fil_val<<8)+lap_fil_val; // RGB�����l������

			if (x<2 || y<2) // �ŏ���2�s�Ƃ��̑��̍s�̍ŏ���2��͖����f�[�^�Ȃ̂�0�Ƃ���
				lap.data = 0;

			if (x==0 && y==0) // �ŏ��̃f�[�^�ł́ATUSER���A�T�[�g����
				lap.user = 1;
			else
				lap.user = 0;
			
			if (x == (HORIZONTAL_PIXEL_WIDTH-1))	// �s�̍Ō�� TLAST ���A�T�[�g����
				lap.last = 1;
			else
				lap.last = 0;

			outs << lap;	// AXI4-Stream �֏o��
		}
	}

	return 0;
}

// RGB����Y�ւ̕ϊ�
// RGB�̃t�H�[�}�b�g�́A{8'd0, R(8bits), G(8bits), B(8bits)}, 1pixel = 32bits
// �P�x�M��Y�݂̂ɕϊ�����B�ϊ����́AY =  0.299R + 0.587G + 0.114B
// "YUV�t�H�[�}�b�g�y�� YUV<->RGB�ϊ�"���Q�l�ɂ����Bhttp://vision.kuee.kyoto-u.ac.jp/~hiroaki/firewire/yuv.html
//�@2013/09/27 : float ���~�߂āA���ׂ�int �ɂ���
int conv_rgb2y(int rgb){
    int r, g, b, y_f;
    int y;

    b = rgb & 0xff;
    g = (rgb>>8) & 0xff;
    r = (rgb>>16) & 0xff;

    y_f = 77*r + 150*g + 29*b; //y_f = 0.299*r + 0.587*g + 0.114*b;�̌W����256�{����
    y = y_f >> 8; // 256�Ŋ���

    return(y);
}

// ���v���V�A���t�B���^
// x0y0 x1y0 x2y0 -1 -1 -1
// x0y1 x1y1 x2y1 -1  8 -1
// x0y2 x1y2 x2y2 -1 -1 -1
int laplacian_fil(int x0y0, int x1y0, int x2y0, int x0y1, int x1y1, int x2y1, int x0y2, int x1y2, int x2y2)
{
	int y;

	y = -x0y0 -x1y0 -x2y0 -x0y1 +8*x1y1 -x2y1 -x0y2 -x1y2 -x2y2;
	if (y<0)
		y = 0;
	else if (y>255)
		y = 255;
    return(y);
}
