#include "stdafx.h"
#include <fstream>
#include <omp.h>
#include "mpi.h"
using namespace std;
struct pixel
	{
	char r;
	char g;
	char b;
	int operator>(pixel tmp)
	{
		if (r==tmp.r)
			if (g==tmp.g)
				if (b>tmp.b) return 1;
				else return 0;
			else if(g>tmp.g) return 1;
			else return 0;
		else if(r>tmp.r) return 1;
		else return 0;
		return 0;
	};
	void operator=(char *tmp)
	{
		r=*(tmp++);
		g=*(tmp++);
		b=*(tmp);
	}
};
class bitmap
{
	char head[0x36];
	int padding;
public:
	int w,h;
	char **bits;
	bitmap()
	{
	}

	int open(char *name)
	{
		fstream  input (name, ios::in | ios::binary);
		if (!input.is_open())
			return 0;
		input.read(head, 0x36);
		w = (int)((((((head[0x15] << 8) | head[0x14]) << 8) | head[0x13]) << 8) | (head[0x12]&0x000000FF));
		h = (int)((((((head[0x19] << 8) | head[0x18]) << 8) | head[0x17]) << 8) | (head[0x16]&0x000000FF));
		padding = 4 - ((w * 3) % 4);
		if(padding == 4) padding = 0;
		bits = new char *[h];
		for(int i =0; i<h;i++)
		{
			bits[i] = new char[w*3];
			input.read(bits[i], w*3);
			int c = input.tellp();
			input.seekp(c + padding);
		}
		input.close();
		return 1;
	}

	int save(char * name,char *out)
	{
		ofstream output;
		output.open(name, ios::out | ios::binary);
	    output.write(head, 0x36);
		for(int i=0; i<h;i++)
		{
			output.write(out, w*3);
			output.write("\0\0\0\0",padding);
			*(out+=w*3);
		}
		output.close();
		return 1;
	}
	void median(int i, int j,int r) 
	{		
		pixel * sortArr;
		pixel temp;
		bool changed;
		
		int area = (2*r+1) * (2*r+1);
		int index = 0;
		sortArr = (pixel*) malloc (sizeof(pixel)*(area));

		for (int k = i-r; k <= i+r; k++)
			for (int l = j-r; l <= j+r; l++) {
				sortArr[index].r = bits[k][3*l];
				sortArr[index].g = bits[k][3*l+1];
				sortArr[index].b = bits[k][3*l+2];
				index++;
			}

		do
		{
			changed = false;
			for (int k = 0; k < area-1; k++)
				if (sortArr[k] > sortArr[k+1]) {
					temp.r = sortArr[k+1].r;
					temp.g = sortArr[k+1].g;
					temp.b = sortArr[k+1].b;

					sortArr[k+1].r = sortArr[k].r;
					sortArr[k+1].g = sortArr[k].g;
					sortArr[k+1].b = sortArr[k].b;

					sortArr[k].r = temp.r;
					sortArr[k].g = temp.g;
					sortArr[k].b = temp.b;
					
					changed = true;
				};
		} while (changed == true);

		int mid = area/2;
		bits[i][3*j] = sortArr[mid].r;
		bits[i][3*j+1] = sortArr[mid].g;
		bits[i][3*j+2] = sortArr[mid].b;
	}
};

int main(int argc, char* argv[])
{
	char *name,*stream;
	int i,j,k,size,chunk,rank,radius;
	double start = 0.0, finish = 0.0;
	k=0;
	if (argc>1) name=argv[1];
	if (argc>2) radius=argv[2];
	start = omp_get_wtime();
	bitmap image;
	omp_set_num_threads(omp_get_num_procs());
	if (!(image.open(name)))
	{
		printf("Error opening file\n");
		return 0;
	}
	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	MPI_Comm_size(MPI_COMM_WORLD,&size);
	chunk=image.h/size;
	stream = new char [chunk*image.w*3];
	if (rank==0) printf("The image is %d pixels wide and %d pixels tall\n",image.w,image.h);
	printf("P:%d is proceeding with %d threads.\n",rank,omp_get_num_threads());
	#pragma omp parallel for private(j)
	for(i = (chunk*rank); i < (chunk*(rank+1)); i++)
		for(j = 0; j < image.w*3; j+=3)
			if ((i>0)&&(i<(image.h-1))&&(j>0)&&(j<((image.w*3)-3))) image.median(i,j,radius);
	for(i = (chunk*rank); i < (chunk*(rank+1)); i++)
		for(j = 0; j < image.w*3; j++)
			stream[k++]=image.bits[i][j];
	char *rbuf; 
	if (rank==0) rbuf =(char*)malloc(image.h*image.w*3*sizeof(char)); 
    MPI_Gather(stream,chunk*image.w*3,MPI_CHAR,rbuf,chunk*image.w*3,MPI_CHAR,0,MPI_COMM_WORLD);
	if (rank !=0) MPI_Finalize();
	finish = omp_get_wtime();
	if (rank == 0) 
	{ 
		printf("Filtering time:\t%4.5f s\n",finish-start);
		image.save("end.bmp",rbuf);
		MPI_Finalize();
	}
	return 1;
	
}