// lap_filter2_axis_tb.cpp
// 2015/05/01
// 2015/08/17 : BMPファイルを読み書きするように変更した
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ap_int.h>
#include <hls_stream.h>
#include <iostream>
#include <fstream>
#include <ap_axi_sdata.h>

#include "lap_filter_axis.h"
#include "bmp_header.h"

int lap_filter_axis(hls::stream<ap_axis<32,1,1,1> >& ins, hls::stream<ap_axis<32,1,1,1> >& outs);

int laplacian_fil_soft(int x0y0, int x1y0, int x2y0, int x0y1, int x1y1, int x2y1, int x0y2, int x1y2, int x2y2);
int conv_rgb2y_soft(int rgb);
int lap_filter_axis_soft(hls::stream<ap_axis<32,1,1,1> >& ins, hls::stream<ap_axis<32,1,1,1> >& outs, int width, int height);

#define CLOCK_PERIOD 10

int main()
{
	using namespace std;

	hls::stream<ap_axis<32,1,1,1> > ins;
	hls::stream<ap_axis<32,1,1,1> > ins_soft;
    hls::stream<ap_axis<32,1,1,1> > outs;
    hls::stream<ap_axis<32,1,1,1> > outs_soft;
	ap_axis<32,1,1,1> pix;
    ap_axis<32,1,1,1> vals;
    ap_axis<32,1,1,1> vals_soft;

    BITMAPFILEHEADER bmpfhr; // BMPファイルのファイルヘッダ(for Read)
    BITMAPINFOHEADER bmpihr; // BMPファイルのINFOヘッダ(for Read)
    FILE *fbmpr, *fbmpw;
    int *rd_bmp, *hw_lapd;
    int blue, green, red;

    if ((fbmpr = fopen("test.bmp", "rb")) == NULL){ // test.bmp をオープン
        fprintf(stderr, "Can't open test.bmp by binary read mode\n");
        exit(1);
    }
    // bmpヘッダの読み出し
    fread(&bmpfhr.bfType, sizeof(char), 2, fbmpr);
    fread(&bmpfhr.bfSize, sizeof(long), 1, fbmpr);
    fread(&bmpfhr.bfReserved1, sizeof(short), 1, fbmpr);
    fread(&bmpfhr.bfReserved2, sizeof(short), 1, fbmpr);
    fread(&bmpfhr.bfOffBits, sizeof(long), 1, fbmpr);
    fread(&bmpihr, sizeof(BITMAPINFOHEADER), 1, fbmpr);

    // ピクセルを入れるメモリをアロケートする
    if ((rd_bmp =(int *)malloc(sizeof(int) * (bmpihr.biWidth * bmpihr.biHeight))) == NULL){
        fprintf(stderr, "Can't allocate rd_bmp memory\n");
        exit(1);
    }
    if ((hw_lapd =(int *)malloc(sizeof(int) * (bmpihr.biWidth * bmpihr.biHeight))) == NULL){
        fprintf(stderr, "Can't allocate hw_lapd memory\n");
        exit(1);
    }

    // rd_bmp にBMPのピクセルを代入。その際に、行を逆転する必要がある
    for (int y=0; y<bmpihr.biHeight; y++){
        for (int x=0; x<bmpihr.biWidth; x++){
            blue = fgetc(fbmpr);
            green = fgetc(fbmpr);
            red = fgetc(fbmpr);
            rd_bmp[((bmpihr.biHeight-1)-y)*bmpihr.biWidth+x] = (blue & 0xff) | ((green & 0xff)<<8) | ((red & 0xff)<<16);
        }
    }
    fclose(fbmpr);

    // ins に入力データを用意する
    for(int i=0; i<5; i++){	//　dummy data
       	pix.user = 0;
     	pix.data = i;
    	ins << pix;
    }

    for(int j=0; j < bmpihr.biHeight; j++){
        for(int i=0; i < bmpihr.biWidth; i++){
        	pix.data = (ap_int<32>)rd_bmp[(j*bmpihr.biWidth)+i];

            if (j==0 && i==0)	// 最初のデータの時に TUSER を 1 にする
            	pix.user = 1;
            else
            	pix.user = 0;

            if (i == bmpihr.biWidth-1) // 行の最後でTLASTをアサートする
                pix.last = 1;
            else
                pix.last = 0;

            ins << pix;
            ins_soft << pix;
        }
    }

    lap_filter_axis(ins, outs);
    lap_filter_axis_soft(ins_soft, outs_soft, bmpihr.biWidth, bmpihr.biHeight);

    // ハードウェアとソフトウェアのラプラシアン・フィルタの値のチェック
    cout << endl;
    cout << "outs" << endl;
    for(int j=0; j < bmpihr.biHeight; j++){
        for(int i=0; i < bmpihr.biWidth; i++){
        	outs >> vals;
            outs_soft >> vals_soft;
            ap_int<32> val = vals.data;
            ap_int<32> val_soft = vals_soft.data;

            hw_lapd[(j*bmpihr.biWidth)+i] = (int)val;

            if (val != val_soft){
                printf("ERROR HW and SW results mismatch i = %ld, j = %ld, HW = %d, SW = %d\n", i, j, (int)val, (int)val_soft);
                return(1);
            }
            if (vals.last)
            	cout << "AXI-Stream is end" << endl;
        }
    }
    cout << "Success HW and SW results match" << endl;
    cout << endl;

    // ハードウェアのラプラシアンフィルタの結果を test_lap.bmp へ出力する
    if ((fbmpw=fopen("test_lap.bmp", "wb")) == NULL){
        fprintf(stderr, "Can't open test_lap.bmp by binary write mode\n");
        exit(1);
    }
    // BMPファイルヘッダの書き込み
    fwrite(&bmpfhr.bfType, sizeof(char), 2, fbmpw);
    fwrite(&bmpfhr.bfSize, sizeof(long), 1, fbmpw);
    fwrite(&bmpfhr.bfReserved1, sizeof(short), 1, fbmpw);
    fwrite(&bmpfhr.bfReserved2, sizeof(short), 1, fbmpw);
    fwrite(&bmpfhr.bfOffBits, sizeof(long), 1, fbmpw);
    fwrite(&bmpihr, sizeof(BITMAPINFOHEADER), 1, fbmpw);

    // RGB データの書き込み、逆順にする
    for (int y=0; y<bmpihr.biHeight; y++){
        for (int x=0; x<bmpihr.biWidth; x++){
            blue = hw_lapd[((bmpihr.biHeight-1)-y)*bmpihr.biWidth+x] & 0xff;
            green = (hw_lapd[((bmpihr.biHeight-1)-y)*bmpihr.biWidth+x] >> 8) & 0xff;
            red = (hw_lapd[((bmpihr.biHeight-1)-y)*bmpihr.biWidth+x]>>16) & 0xff;

            fputc(blue, fbmpw);
            fputc(green, fbmpw);
            fputc(red, fbmpw);
        }
    }
    fclose(fbmpw);
    free(rd_bmp);
    free(hw_lapd);

    return 0;
}

int lap_filter_axis_soft(hls::stream<ap_axis<32,1,1,1> >& ins, hls::stream<ap_axis<32,1,1,1> >& outs, int width, int height){
    ap_axis<32,1,1,1> pix;
    ap_axis<32,1,1,1> lap;
    unsigned int **line_buf;
    int pix_mat[3][3];
    int lap_fil_val;
    int i;

    // line_buf の1次元目の配列をアロケートする
    if ((line_buf =(unsigned int **)malloc(sizeof(unsigned int *) * 2)) == NULL){
        fprintf(stderr, "Can't allocate line_buf[3][]\n");
        exit(1);
    }

    // メモリをアロケートする
    for (i=0; i<2; i++){
        if ((line_buf[i]=(unsigned int *)malloc(sizeof(unsigned int) * width)) == NULL){
            fprintf(stderr, "Can't allocate line_buf[%d]\n", i);
            exit(1);
        }
    }

    do {    // user が 1になった時にフレームがスタートする
        ins >> pix;
    } while(pix.user == 0);

    for (int y=0; y<height; y++){
        for (int x=0; x<width; x++){
            if (!(x==0 && y==0))    // 最初の入力はすでに入力されている
                ins >> pix; // AXI4-Stream からの入力

            for (int k=0; k<3; k++){
                for (int m=0; m<2; m++){
                    pix_mat[k][m] = pix_mat[k][m+1];
                }
            }
            pix_mat[0][2] = line_buf[0][x];
            pix_mat[1][2] = line_buf[1][x];

            int y_val = conv_rgb2y_soft(pix.data);
            pix_mat[2][2] = y_val;

            line_buf[0][x] = line_buf[1][x];    // 行の入れ替え
            line_buf[1][x] = y_val;

            lap_fil_val = laplacian_fil_soft(    pix_mat[0][0], pix_mat[0][1], pix_mat[0][2],
                                        pix_mat[1][0], pix_mat[1][1], pix_mat[1][2], 
                                        pix_mat[2][0], pix_mat[2][1], pix_mat[2][2]);
            lap.data = (lap_fil_val<<16)+(lap_fil_val<<8)+lap_fil_val; // RGB同じ値を入れる

            if (x<2 || y<2) // 最初の2行とその他の行の最初の2列は無効データなので0とする
                lap.data = 0;

            if (x==0 && y==0) // 最初のデータでは、TUSERをアサートする
                lap.user = 1;
            else
                lap.user = 0;
            
            if (x == (HORIZONTAL_PIXEL_WIDTH-1))    // 行の最後で TLAST をアサートする
                lap.last = 1;
            else
                lap.last = 0;

            outs << lap;    // AXI4-Stream へ出力
        }
    }

    for (i=0; i<2; i++)
        free(line_buf[i]);
    free(line_buf);

    return 0;
}

// RGBからYへの変換
// RGBのフォーマットは、{8'd0, R(8bits), G(8bits), B(8bits)}, 1pixel = 32bits
// 輝度信号Yのみに変換する。変換式は、Y =  0.299R + 0.587G + 0.114B
// "YUVフォーマット及び YUV<->RGB変換"を参考にした。http://vision.kuee.kyoto-u.ac.jp/~hiroaki/firewire/yuv.html
//　2013/09/27 : float を止めて、すべてint にした
int conv_rgb2y_soft(int rgb){
    int r, g, b, y_f;
    int y;

    b = rgb & 0xff;
    g = (rgb>>8) & 0xff;
    r = (rgb>>16) & 0xff;

    y_f = 77*r + 150*g + 29*b; //y_f = 0.299*r + 0.587*g + 0.114*b;の係数に256倍した
    y = y_f >> 8; // 256で割る

    return(y);
}

// ラプラシアンフィルタ
// x0y0 x1y0 x2y0 -1 -1 -1
// x0y1 x1y1 x2y1 -1  8 -1
// x0y2 x1y2 x2y2 -1 -1 -1
int laplacian_fil_soft(int x0y0, int x1y0, int x2y0, int x0y1, int x1y1, int x2y1, int x0y2, int x1y2, int x2y2)
{
    int y;

    y = -x0y0 -x1y0 -x2y0 -x0y1 +8*x1y1 -x2y1 -x0y2 -x1y2 -x2y2;
    if (y<0)
        y = 0;
    else if (y>255)
        y = 255;
    return(y);
}
