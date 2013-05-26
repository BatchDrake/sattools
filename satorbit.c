#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <getopt.h>
#include "cpgplot.h"
#include "sgdp4h.h"

#define LIM 80
#define NMAX 1024
#define MMAX 28368
#define D2R M_PI/180.0
#define R2D 180.0/M_PI
#define XKMPER 6378.135 // Earth radius in km
#define FLAT (1.0/298.257)
#define XKMPAU 149597879.691 // AU in km

long Isat=0;
long Isatsel=0;
extern double SGDP4_jd0;

struct map {
  long satno;
  double l0,b0,h0;
  double lat,lng;
  double mjd;
  float alt,timezone;
  int length;
  char orientation[LIM];
  char nfd[LIM],tlefile[LIM],observer[32];
  char datadir[LIM],tledir[LIM];
  int site_id;
  float w;
} m;
struct globe {
  int n;
  float l[MMAX],b[MMAX],x[MMAX],y[MMAX],z[MMAX];
} glb;
struct sat {
  long Isat;
  double jd;
  double dx,dy,dz;
  double x,y,z,vx,vy,vz;
  double rsun,rearth;
  double psun,pearth,p;
  double r,ra,de;
  double azi,alt;
  double rx,ry;
  double lng,lat;
};
void read_globe(void);
void plot_globe(void);
double nfd2mjd(char *date);
double date2mjd(int year,int month,double day);
void mjd2date(double mjd,char *date,int length);
void usage();
void nfd_now(char *s);
void rotate(int axis,float angle,float *x,float *y,float *z);
void sunpos_xyz(double mjd,xyz_t *pos,double *ra,double *de);
double gmst(double);
double dgmst(double);
double modulo(double,double);
void get_site(int site_id);

// Initialize setup
void initialize_setup(void)
{
  char *env;

  // Default parameters
  m.satno=0;
  m.timezone=0.0;
  m.length=60;
  strcpy(m.orientation,"terrestial");
  nfd_now(m.nfd);
  m.mjd=nfd2mjd(m.nfd);
  m.w=1.2;
  m.h0=gmst(m.mjd);

  // Default settings
  strcpy(m.observer,"Unknown");
  m.site_id=0;

  // Get environment variables
  env=getenv("ST_DATADIR");
  if (env!=NULL) {
    strcpy(m.datadir,env);
  } else {
    printf("ST_DATADIR environment variable not found.\n");
  }
  env=getenv("ST_COSPAR");
  if (env!=NULL) {
    get_site(atoi(env));
  } else {
    printf("ST_COSPAR environment variable not found.\n");
  }
  env=getenv("ST_TLEDIR");
  if (env!=NULL) {
    strcpy(m.tledir,env);
  } else {
    printf("ST_TLEDIR environment variable not found.\n");
  }
  sprintf(m.tlefile,"%s/classfd.tle",m.tledir);

  return;
}

void plot_footprint(struct sat s)
{
  int i,j,flag;
  float range,alpha,dist,zz,theta,rr;
  float x,y,z,x0,y0,z0,r0,r;

  // Foot print size
  range=sqrt(s.x*s.x+s.y*s.y+s.z*s.z);
  dist=sqrt(range*range-XKMPER*XKMPER);
  alpha=acos(XKMPER/range);
  zz=range-dist*sin(alpha);
  rr=sqrt(XKMPER*XKMPER-zz*zz);

  // Sub satellite point
  z=cos(s.lng*D2R)*cos(s.lat*D2R)*XKMPER;
  x=sin(s.lng*D2R)*cos(s.lat*D2R)*XKMPER;
  y=sin(s.lat*D2R)*XKMPER;
  rotate(1,m.l0,&x,&y,&z);
  rotate(0,m.b0,&x,&y,&z);
  r=sqrt(x*x+y*y);
  z0=cos(s.lng*D2R)*cos(s.lat*D2R)*range;
  x0=sin(s.lng*D2R)*cos(s.lat*D2R)*range;
  y0=sin(s.lat*D2R)*range;
  rotate(1,m.l0,&x0,&y0,&z0);
  rotate(0,m.b0,&x0,&y0,&z0);
  r0=sqrt(x0*x0+y0*y0);

  if (r<XKMPER && z<0.0) {
    x*=XKMPER/r;
    y*=XKMPER/r;
  }
  if (r0>XKMPER || (r0<XKMPER && z0>0.0)) {
    cpgmove(x0,y0);
    cpgdraw(x,y);
  }
  if (z>0.0) 
    cpgpt1(x,y,4);

  for (i=0,j=0,flag=0;i<NMAX;i++,j++) {
    theta=2.0*M_PI*(float) i/(float) (NMAX-1);
      
    x=rr*sin(theta);
    y=rr*cos(theta);
    z=zz;

    rotate(0,-s.lat,&x,&y,&z);
    rotate(1,-s.lng,&x,&y,&z);
    rotate(1,m.l0,&x,&y,&z);
    rotate(0,m.b0,&x,&y,&z);
    
    if (flag==0)
      cpgmove(x,y);
    else
      cpgdraw(x,y);
    
    if (z>0.0) 
      flag=1;
    else
      flag=0;
  }

  return;
}

// Computes apparent position
struct sat apparent_position(double mjd)
{
  struct sat s;
  double jd,rsun,rearth;
  double dx,dy,dz;
  xyz_t satpos,obspos,satvel,sunpos;
  double sra,sde;

  // Sat ID
  s.Isat=Isat;

  // Get Julian Date
  jd=mjd+2400000.5;

  // Get positions
  satpos_xyz(jd,&satpos,&satvel);
  sunpos_xyz(mjd,&sunpos,&sra,&sde);

  // Sat positions
  s.x=satpos.x;
  s.y=satpos.y;
  s.z=satpos.z;
  s.vx=satvel.x;
  s.vy=satvel.y;
  s.vz=satvel.y;

  // Sun position from satellite
  dx=-satpos.x+sunpos.x;  
  dy=-satpos.y+sunpos.y;
  dz=-satpos.z+sunpos.z;

  // Distances
  rsun=sqrt(dx*dx+dy*dy+dz*dz);
  rearth=sqrt(satpos.x*satpos.x+satpos.y*satpos.y+satpos.z*satpos.z);
  // Angles
  s.psun=asin(696.0e3/rsun)*R2D;
  s.pearth=asin(6378.135/rearth)*R2D;
  s.p=acos((-dx*satpos.x-dy*satpos.y-dz*satpos.z)/(rsun*rearth))*R2D;
  //  s.p=acos(((sunpos.x+satpos.x)*satpos.x+(sunpos.y+satpos.y)*satpos.y+(sunpos.z+satpos.z)*satpos.z)/(rsun*rearth))*R2D;

  s.p-=s.pearth;

  // Celestial position
  s.r=sqrt(satpos.x*satpos.x+satpos.y*satpos.y+satpos.z*satpos.z);
  s.ra=atan2(satpos.y,satpos.x)*R2D;
  s.de=asin(satpos.z/s.r)*R2D;

  // Latitude and longitude
  s.lng=s.ra-gmst(m.mjd);;
  s.lat=s.de;
  
  return s;
}

// plot satellite track
void plot_track(void)
{
  int i=0,nstep=500;
  orbit_t orb;
  xyz_t pos,vel;
  double jd,dt,h,mjd;
  FILE *fp=NULL;
  float x,y,z,r,v;
  long imode;
  int isci;
  float isch;
  char norad[7];
  struct sat s;

  cpgqci(&isci);
  cpgqch(&isch);
  cpgsci(7);

  fp=fopen(m.tlefile,"rb");
  if (fp==NULL) {
    fatal_error("File open failed for reading \"%s\"",m.tlefile);
  }
  
  while (read_twoline(fp,m.satno,&orb) == 0) {
    //    print_orb(&orb);
    
    Isat=orb.satno;
    imode=init_sgdp4(&orb);
    
    if(imode == SGDP4_ERROR) continue;

    jd=m.mjd+2400000.5;
    h=gmst(m.mjd);

    for (i=0,dt=0.0;;i++) {
      //if(satpos_xyz(jd, &pos, &vel) == SGDP4_ERROR) break;
      mjd=jd-2400000.5;
      s=apparent_position(mjd);
      
      x=s.x;
      y=s.y;
      z=s.z;

      rotate(0,-90.0,&x,&y,&z);
      rotate(1,90.0,&x,&y,&z);
      rotate(1,m.l0+h,&x,&y,&z);
      rotate(0,m.b0,&x,&y,&z);
      
      // Visibility
      if (s.p<-s.psun)
	cpgsci(14);
      else if (s.p>-s.psun && s.p<s.psun)
	cpgsci(15);
      else if (s.p>s.psun)
	cpgsci(7);
      
      // Plot
      if (i==0) {
	plot_footprint(s);
	if (!(sqrt(x*x+y*y)<XKMPER && z<0.0)) {
	  sprintf(norad," %ld",Isat);
	  cpgsch(0.6);
	  cpgtext(x,y,norad);
	  cpgsch(isch);
	  cpgpt1(x,y,17);
	}
	cpgmove(x,y);
      } else {
	if (sqrt(x*x+y*y)<XKMPER && z<0.0)
	  cpgmove(x,y);
	else
	  cpgdraw(x,y);
      }

      // Do timestep
      r=sqrt(s.x*s.x+s.y*s.y+s.z*s.z);
      v=sqrt(s.vx*s.vx+s.vy*s.vy+s.vz*s.vz);
      dt=2.0*M_PI*r/(0.75*v*nstep);
      jd+=dt/86400.0;

      if (i==nstep)
	break;
    }
  }
  cpgsls(1);
  cpgsci(isci);
  cpgsch(isch);


  return;
}

void read_globe(void)
{
  int i,status;
  FILE *file;
  float l,b;
  char filename[LIM];

  sprintf(filename,"%s/data/globe.dat",m.datadir);
  file=fopen(filename,"r");

  for (i=0;i<MMAX;i++) {
    status=fscanf(file,"%f %f",&glb.b[i],&glb.l[i]);

    l=glb.l[i]*D2R;
    b=glb.b[i]*D2R;

    glb.z[i]=XKMPER*cos(l)*cos(b);
    glb.x[i]=XKMPER*sin(l)*cos(b);
    glb.y[i]=XKMPER*sin(b);
  }
  fclose(file);
  glb.n=MMAX;

  return;
}


void plot_globe(void)
{
  int i,flag;
  float x,y,z;

  for (i=0,flag=0;i<glb.n;i++) {
    if (glb.b[i]==9999.0) {
      flag=0;
      continue;
    }
    x=glb.x[i];
    y=glb.y[i];
    z=glb.z[i];

    rotate(1,m.l0,&x,&y,&z);
    rotate(0,m.b0,&x,&y,&z);
    
    if (z>0.0) {
      if (flag==0) {
	cpgmove(x,y);
	flag=1;
      } else {
	cpgdraw(x,y);
      }
    } else {
      flag=0;
    }
  }

  return;
}

// plot grid
void plot_grid(void)
{
  int i,j,flag;
  float l,b;
  float x,y,z;

  for (l=0.0;l<=360.0;l+=30.0) {
    for (b=-90.0,flag=0;b<=90.0;b+=1.0) {
      z=cos(l*D2R)*cos(b*D2R)*XKMPER;
      x=sin(l*D2R)*cos(b*D2R)*XKMPER;
      y=sin(b*D2R)*XKMPER;
    
      rotate(1,m.l0,&x,&y,&z);
      rotate(0,m.b0,&x,&y,&z);

      if (flag==0)
	cpgmove(x,y);
      else
	cpgdraw(x,y);

      if (z>0.0) 
	flag=1;
      else
	flag=0;
    }
  }

  for (b=-90.0;b<=90.0;b+=30.0) {
    for (l=0.0,flag=0;l<=360.0;l+=1.0) {
      z=cos(l*D2R)*cos(b*D2R)*XKMPER;
      x=sin(l*D2R)*cos(b*D2R)*XKMPER;
      y=sin(b*D2R)*XKMPER;
    
      rotate(1,m.l0,&x,&y,&z);
      rotate(0,m.b0,&x,&y,&z);

      if (flag==0)
	cpgmove(x,y);
      else
	cpgdraw(x,y);

      if (z>0.0) 
	flag=1;
      else
	flag=0;
    }
  }

  return;
}

// Plot terminator
void plot_terminator(void)
{
  int i,j,k,flag,j1,j2;
  double jd;
  xyz_t s;
  float r,h;
  float l,b,l0,b0;
  float x0,y0,z0;
  float x,y,z,t0,t1,t2,t;
  float xx[NMAX],yy[NMAX],zz[NMAX];
  float xt[NMAX],yt[NMAX],zt[NMAX];
  int isci;
  double sra,sde;
  float theta;
  float ang[]={0.0,-6.0,-12.0,-18.0};

  cpgqci(&isci);

  // Get positions
  sunpos_xyz(m.mjd,&s,&sra,&sde);

  // GMST
  h=gmst(m.mjd);

  // Solar subpoint
  l0=modulo(sra-h,360.0);
  b0=sde;
  if (l0>180.0)
    l0-=360.0;

  // Convert
  z0=cos(l0*D2R)*cos(b0*D2R)*XKMPER;
  x0=sin(l0*D2R)*cos(b0*D2R)*XKMPER;
  y0=sin(b0*D2R)*XKMPER;

  rotate(1,m.l0,&x0,&y0,&z0);
  rotate(0,m.b0,&x0,&y0,&z0);    

  t0=atan2(y0,x0)*R2D;

  // Loop over terminator boundaries
  for (i=0,j=0,flag=0;i<NMAX;i++,j++) {
    theta=2.0*M_PI*(float) i/(float) (NMAX-1);
    
    x=XKMPER*sin(theta);
    y=XKMPER*cos(theta);
    z=0.0;
    
    rotate(0,-b0,&x,&y,&z);
    rotate(1,-l0,&x,&y,&z);
    rotate(1,m.l0,&x,&y,&z);
    rotate(0,m.b0,&x,&y,&z);
    xx[i]=x;
    yy[i]=y;
    zz[i]=z;
  }
  for (i=0,j=0;i<NMAX;i++) {
    if (i>0 && zz[i]*zz[i-1]<0.0) {
      if (zz[i]>0.0 && zz[i-1]<0.0) {
	t1=atan2(yy[i],xx[i])*R2D;
	j1=i;
      } else {
	t2=atan2(yy[i],xx[i])*R2D;
	j2=i;
      }
    }
  }
  // angles
  t0=modulo(t0,360);
  t1=modulo(t1,360);
  t2=modulo(t2,360);
  if (t1<t2)
    t1+=360.0;
  if (abs(j2-j1)>512)
    j2++;
  if (abs(j2-j1)<512)
    j2--;
  if (j1>j2) {
    for (i=0,j=0;i<j2;i++,j++) {
      xt[j]=xx[i];
      yt[j]=yy[i];
    }
    for (i=0;i<NMAX-abs(j2-j1);i++,j++) {
      t=t2-(t2-t1)*(float) i/(float) (NMAX-abs(j2-j1));
      
      xt[j]=XKMPER*cos(t*D2R);
      yt[j]=XKMPER*sin(t*D2R);
    }
    for (i=j1;i<NMAX;i++,j++) {
      xt[j]=xx[i];
      yt[j]=yy[i];
    }
  } else {
    for (i=j1,j=0;i<j2;i++,j++) {
      xt[j]=xx[i];
      yt[j]=yy[i];
    }
    for (i=0;i<NMAX-abs(j2-j1);i++,j++) {
      t=t2-(t2-t1)*(float) i/(float) (NMAX-abs(j2-j1));
      
      xt[j]=XKMPER*cos(t*D2R);
      yt[j]=XKMPER*sin(t*D2R);
    }
  }

  // Plot day side
  cpgscr(17,0.0,0.0,0.7);
  cpgsci(17);
  cpgsfs(1);
  cpgcirc(0.0,0.0,XKMPER);

  // Plot night side
  cpgscr(16,0.0,0.0,0.2);
  cpgsci(16);
  cpgpoly(NMAX,xt,yt);

  // Plot
  if (z0>0.0) {
    cpgsci(7);
    cpgpt1(x0,y0,17);
  }

  // Loop over terminator boundaries
  for (k=0;k<4;k++) {
    if (k==0)
      cpgsci(2);
    else
      cpgsci(4);
    for (i=0,j=0,flag=0;i<NMAX;i++,j++) {
      theta=2.0*M_PI*(float) i/(float) (NMAX-1);
      
      x=XKMPER*sin(theta)*cos(ang[k]*D2R);
      y=XKMPER*cos(theta)*cos(ang[k]*D2R);
      z=XKMPER*sin(ang[k]*D2R);
      
      rotate(0,-b0,&x,&y,&z);
      rotate(1,-l0,&x,&y,&z);
      rotate(1,m.l0,&x,&y,&z);
      rotate(0,m.b0,&x,&y,&z);

      if (flag==0)
	cpgmove(x,y);
      else
	cpgdraw(x,y);

      if (z>0.0) 
	flag=1;
      else
	flag=0;
    }
  }
  cpgsci(isci);
  return;
}

void plot_map(void)
{
  int redraw=1,status;
  char text[256];
  float x,y,z;
  char c;

  for (;;) {
    if (redraw>0) {
      // Get present mjd
      if (m.mjd<0.0) {
	nfd_now(m.nfd);
	m.mjd=nfd2mjd(m.nfd);
	m.h0=gmst(m.mjd);
      }

      // Update position
      if (strcmp(m.orientation,"terrestial")==0) {
	m.l0=m.lng;
	m.b0=m.lat;
      } else if (strcmp(m.orientation,"sidereal")==0) {
	m.l0=m.lng-gmst(m.mjd)+m.h0;
	m.b0=m.lat;
      }

      cpgscr(0,0.0,0.0,0.0);
      cpgeras();

      // Create window
      cpgsvp(0.05,0.95,0.05,0.95);
      cpgwnad(-m.w*XKMPER,m.w*XKMPER,-m.w*XKMPER,m.w*XKMPER);

      // Set background
      cpgscr(0,0.0,0.0,0.5);
      cpgsci(0);
      cpgwnad(-m.w*XKMPER,m.w*XKMPER,-m.w*XKMPER,m.w*XKMPER);
      cpgsci(1);
      cpgscr(0,0.0,0.0,0.0);

      // Top left string
      cpgsch(0.8);
      mjd2date(m.mjd,m.nfd,0);
      sprintf(text,"%s UTC",m.nfd);
      cpgmtxt("T",0.6,0.0,0.0,text);

      // Bottom string
      sprintf(text,"l: %d s",m.length);
      cpgmtxt("B",1.0,0.0,0.0,text);
      cpgsch(1.0);

      // Plot terminator
      plot_terminator();

      // Plot Grid
      cpgsls(2);
      cpgsci(14);
      plot_grid();
      cpgsls(1);
      cpgsci(1);

      // Plot globe
      plot_globe();
      cpgsfs(2);
      cpgcirc(0.0,0.0,XKMPER);
      cpgpt1(0.0,0.0,2);
      cpgsci(1);
      cpgbox("BC",0.,0,"BC",0.,0);

      // Plot track
      plot_track();
    }
    
    // Reset redraw
    redraw=0;

    // Get cursor
    cpgcurs(&x,&y,&c);

    // Redraw
    if (c=='r') {
      m.mjd=-1.0;
      m.length=60;
      redraw=1;
    }

    // Orientation
    if (c=='o') {
      if (strcmp(m.orientation,"terrestial")==0)
	strcpy(m.orientation,"sidereal");
      else if (strcmp(m.orientation,"sidereal")==0)
	strcpy(m.orientation,"terrestial");
      redraw=1;
    }

    // Recenter
    if (sqrt(x*x+y*y)<XKMPER && c=='c') {
      z=sqrt(XKMPER*XKMPER-x*x-y*y);
      rotate(0,-m.lat,&x,&y,&z);
      rotate(1,-m.l0,&x,&y,&z);
      rotate(1,-90.0,&x,&y,&z);
      rotate(0,90.0,&x,&y,&z);

      m.lng=atan2(y,x)*R2D;
      m.lat=asin(z/XKMPER)*R2D;

      m.l0=m.lng;
      m.b0=m.lat;
      redraw=1;
    }

    // Zoom
    if (c=='-') {
      m.w*=1.2;
      redraw=1;
    }
    if (c=='+' || c=='=') {
      m.w/=1.2;
      redraw=1;
    }

    // Pan
    if (c=='{') {
      m.lat-=10.0;
      redraw=1;
    }      
    if (c=='}') {
      m.lat+=10.0;
      redraw=1;
    }      
    if (c=='[') {
      m.lng-=10.0;
      redraw=1;
    }
    if (c==']') {
      m.lng+=10.0;
      redraw=1;
    }
    
    if (c=='>') {
      m.length*=2.0;
      redraw=1;
    }
    if (c=='<') {
      m.length/=2.0;
      redraw=1;
    }
    if (c==',') {
      m.mjd-=m.length/86400.0;
      redraw=1;
    }
    if (c=='.') {
      m.mjd+=m.length/86400.0;
      redraw=1;
    }
    
    // Integration lenght
    if (c=='l') {
      printf("Enter integration length (s): ");
      status=scanf("%d",&m.length);
      redraw=1;
    }

    // Exit
    if (c=='q' || c=='Q') {
      cpgend();
      exit(0);
    }
  }

  return;
}

int main(int argc,char *argv[])
{
  int arg=0;

  // Initialize setup
  initialize_setup();

  // Decode options
  while ((arg=getopt(argc,argv,"t:c:i:s:l:h"))!=-1) {
    switch (arg) {
      
    case 't':
      strcpy(m.nfd,optarg);
      m.mjd=nfd2mjd(m.nfd);
      break;

    case 'c':
      strcpy(m.tlefile,optarg);
      break;

    case 's':
      get_site(atoi(optarg));
      break;

    case 'i':
      m.satno=atoi(optarg);
      break;

    case 'l':
      m.length=atoi(optarg);
      break;

    case 'h':
      usage();
      return 0;
      break;

    default:
      usage();
      return 0;
    }
  }

  read_globe();

  cpgopen("/xs");

  plot_map();

  cpgend();


  return 0;
}

// Present nfd
void nfd_now(char *s)
{
  time_t rawtime;
  struct tm *ptm;

  // Get UTC time
  time(&rawtime);
  ptm=gmtime(&rawtime);
    
  sprintf(s,"%04d-%02d-%02dT%02d:%02d:%02d",ptm->tm_year+1900,ptm->tm_mon+1,ptm->tm_mday,ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
  
  return;
}

// nfd2mjd
double nfd2mjd(char *date)
{
  int year,month,day,hour,min,sec;
  double mjd,dday;

  sscanf(date,"%04d-%02d-%02dT%02d:%02d:%02d",&year,&month,&day,&hour,&min,&sec);
  dday=day+hour/24.0+min/1440.0+sec/86400.0;

  mjd=date2mjd(year,month,dday);

  return mjd;
}

void usage()
{
  return;
}

// Compute Date from Julian Day
void mjd2date(double mjd,char *date,int length)
{
  double f,jd,dday;
  int z,alpha,a,b,c,d,e;
  int year,month,day,hour,min;
  float sec,x;

  jd=mjd+2400000.5;
  jd+=0.5;

  z=floor(jd);
  f=fmod(jd,1.);

  if (z<2299161)
    a=z;
  else {
    alpha=floor((z-1867216.25)/36524.25);
    a=z+1+alpha-floor(alpha/4.);
  }
  b=a+1524;
  c=floor((b-122.1)/365.25);
  d=floor(365.25*c);
  e=floor((b-d)/30.6001);

  dday=b-d-floor(30.6001*e)+f;
  if (e<14)
    month=e-1;
  else
    month=e-13;

  if (month>2)
    year=c-4716;
  else
    year=c-4715;

  day=(int) floor(dday);
  x=24.0*(dday-day);
  x=3600.*fabs(x);
  sec=fmod(x,60.);
  x=(x-sec)/60.;
  min=fmod(x,60.);
  x=(x-min)/60.;
  hour=x;
  sec=floor(1000.0*sec)/1000.0;

  if (length==3)
    sprintf(date,"%04d-%02d-%02dT%02d:%02d:%06.3f",year,month,day,hour,min,sec);
  else if (length==0)
    sprintf(date,"%04d-%02d-%02dT%02d:%02d:%02.0f",year,month,day,hour,min,sec);
  return;
}

// Compute Julian Day from Date
double date2mjd(int year,int month,double day)
{
  int a,b;
  double jd;

  if (month<3) {
    year--;
    month+=12;
  }

  a=floor(year/100.);
  b=2.-a+floor(a/4.);

  if (year<1582) b=0;
  if (year==1582 && month<10) b=0;
  if (year==1852 && month==10 && day<=4) b=0;

  jd=floor(365.25*(year+4716))+floor(30.6001*(month+1))+day+b-1524.5;

  return jd-2400000.5;
}

// rotate vector
void rotate(int axis,float angle,float *x,float *y,float *z)
{
  float xx,yy,zz;
  float ca,sa;

  ca=cos(angle*D2R);
  sa=sin(angle*D2R);
  
  if (axis==0) {
    xx= *x;
    yy= *y*ca- *z*sa;
    zz= *z*ca+ *y*sa;
  }
  if (axis==1) {
    xx= *x*ca- *z*sa;
    yy= *y;
    zz= *z*ca+ *x*sa;
  }
  if (axis==2) {
    xx= *x*ca- *y*sa;
    yy= *y*ca+ *x*sa;
    zz= *z;
  }
  
  *x=xx;
  *y=yy;
  *z=zz;

  return;
}

// Solar position
void sunpos_xyz(double mjd,xyz_t *pos,double *ra,double *de)
{
  double jd,t,l0,m,e,c,r;
  double n,s,ecl;

  jd=mjd+2400000.5;
  t=(jd-2451545.0)/36525.0;
  l0=modulo(280.46646+t*(36000.76983+t*0.0003032),360.0)*D2R;
  m=modulo(357.52911+t*(35999.05029-t*0.0001537),360.0)*D2R;
  e=0.016708634+t*(-0.000042037-t*0.0000001267);
  c=(1.914602+t*(-0.004817-t*0.000014))*sin(m)*D2R;
  c+=(0.019993-0.000101*t)*sin(2.0*m)*D2R;
  c+=0.000289*sin(3.0*m)*D2R;

  r=1.000001018*(1.0-e*e)/(1.0+e*cos(m+c));
  n=modulo(125.04-1934.136*t,360.0)*D2R;
  s=l0+c+(-0.00569-0.00478*sin(n))*D2R;
  ecl=(23.43929111+(-46.8150*t-0.00059*t*t+0.001813*t*t*t)/3600.0+0.00256*cos(n))*D2R;

  *ra=atan2(cos(ecl)*sin(s),cos(s))*R2D;
  *de=asin(sin(ecl)*sin(s))*R2D;

  pos->x=r*cos(*de*D2R)*cos(*ra*D2R)*XKMPAU;
  pos->y=r*cos(*de*D2R)*sin(*ra*D2R)*XKMPAU;
  pos->z=r*sin(*de*D2R)*XKMPAU;

  return;
}

// Return x modulo y [0,y)
double modulo(double x,double y)
{
  x=fmod(x,y);
  if (x<0.0) x+=y;

  return x;
}

// Greenwich Mean Sidereal Time
double gmst(double mjd)
{
  double t,gmst;

  t=(mjd-51544.5)/36525.0;

  gmst=modulo(280.46061837+360.98564736629*(mjd-51544.5)+t*t*(0.000387933-t/38710000),360.0);

  return gmst;
}

// Greenwich Mean Sidereal Time
double dgmst(double mjd)
{
  double t,dgmst;

  t=(mjd-51544.5)/36525.0;

  dgmst=360.98564736629+t*(0.000387933-t/38710000);

  return dgmst;
}

// Get observing site
void get_site(int site_id)
{
  int i=0;
  char line[LIM];
  FILE *file;
  int id;
  double lat,lng;
  float alt;
  char abbrev[3],observer[64];
  char filename[LIM];

  sprintf(filename,"%s/data/sites.txt",m.datadir);
  file=fopen(filename,"r");
  if (file==NULL) {
    printf("File with site information not found!\n");
    return;
  }
  while (fgets(line,LIM,file)!=NULL) {
    // Skip
    if (strstr(line,"#")!=NULL)
      continue;

    // Strip newline
    line[strlen(line)-1]='\0';

    // Read data
    sscanf(line,"%4d %2s %lf %lf %f",
	   &id,abbrev,&lat,&lng,&alt);
    strcpy(observer,line+38);

    // Change to km
    alt/=1000.0;
    
    if (id==site_id) {
      m.lat=lat;
      m.lng=lng;
      m.alt=alt;
      m.site_id=id;
      strcpy(m.observer,observer);
    }

  }
  fclose(file);
  
  return;
}
