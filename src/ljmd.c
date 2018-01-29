#include "ljmd.h"
#include "cell.h"

/* main */
int main(int argc, char **argv) 
{
    int nprint, i;
    int rank, nprocs;
    char restfile[BLEN], trajfile[BLEN], ergfile[BLEN], line[BLEN];
    mdsys_t sys; FILE *fp;
#ifdef TIMING
    double timer = 0; // for recording time
#endif

#ifdef USE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#else
    rank = 0;
    nprocs = 1;
#endif    

    // Initialize the struct to zero
    memset(&sys, 0, sizeof(sys));
    
    if (rank == 0) {
      /* read input file */
      if(get_a_line(stdin,line)) return 1;
      sys.natoms=atoi(line);
      if(get_a_line(stdin,line)) return 1;
      sys.redmass = atof(line)*mvsq2e; // reduced mass
      if(get_a_line(stdin,line)) return 1;
      sys.epsilon=atof(line);
      if(get_a_line(stdin,line)) return 1;
      sys.sigma=atof(line);
      if(get_a_line(stdin,line)) return 1;
      sys.rcut=atof(line);
      if(get_a_line(stdin,line)) return 1;
      sys.box=atof(line);
      if(get_a_line(stdin,restfile)) return 1;
      if(get_a_line(stdin,trajfile)) return 1;
      if(get_a_line(stdin,ergfile)) return 1;
      if(get_a_line(stdin,line)) return 1;
      sys.nsteps=atoi(line);
      if(get_a_line(stdin,line)) return 1;
      sys.dt=atof(line);
      if(get_a_line(stdin,line)) return 1;
      nprint=atoi(line);
    }
    
    // Communicate the struct to the rest of the processes
    MPI_Bcast(&sys, sizeof(sys), MPI_CHAR, 0, MPI_COMM_WORLD);
    
    /* allocate memory */
    sys.rx=(double *)malloc(sys.natoms*sizeof(double));
    sys.ry=(double *)malloc(sys.natoms*sizeof(double));
    sys.rz=(double *)malloc(sys.natoms*sizeof(double));
    sys.vx=(double *)malloc(sys.natoms*sizeof(double));
    sys.vy=(double *)malloc(sys.natoms*sizeof(double));
    sys.vz=(double *)malloc(sys.natoms*sizeof(double));
    sys.fx=(double *)malloc(sys.natoms*sizeof(double));
    sys.fy=(double *)malloc(sys.natoms*sizeof(double));
    sys.fz=(double *)malloc(sys.natoms*sizeof(double));

    /* read restart */
    if (rank == 0) {
      fp=fopen(restfile,"r");
      if(fp) {
        for (i=0; i<sys.natoms; ++i) {
	  fscanf(fp,"%lf%lf%lf",sys.rx+i, sys.ry+i, sys.rz+i);
        }
        for (i=0; i<sys.natoms; ++i) {
	  fscanf(fp,"%lf%lf%lf",sys.vx+i, sys.vy+i, sys.vz+i);
        }
        fclose(fp);
        azzero(sys.fx, sys.natoms);
        azzero(sys.fy, sys.natoms);
        azzero(sys.fz, sys.natoms);
      } else {
        perror("cannot read restart file");
        return 3;
      }
    }

    // Broadcast the position and velocity to all processes
    MPI_Bcast(sys.rx, sys.natoms, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(sys.ry, sys.natoms, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(sys.rz, sys.natoms, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    MPI_Bcast(sys.vx, sys.natoms, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(sys.vy, sys.natoms, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(sys.vz, sys.natoms, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    /* initialize forces and energies.*/
    sys.nfi=0;
    force(&sys);

    // Get the result from every process
    
    ekin(&sys);
    
#ifndef TIMING
    FILE *traj,*erg;
    if (rank == 0) {
      erg=fopen(ergfile,"w");
      traj=fopen(trajfile,"w");
      printf("Starting simulation with %d atoms for %d steps.\n",sys.natoms, sys.nsteps);
      printf("     NFI            TEMP            EKIN                 EPOT              ETOT\n");
      output(&sys, erg, traj);
    }
#endif
    

    /**************************************************/
    /* main MD loop */
    
    for(sys.nfi=1; sys.nfi <= sys.nsteps; ++sys.nfi) {
        
        /* propagate system and recompute energies */
#ifdef TIMING
        velverlet_1(&sys);
	if (rank == 0) {
	  timer -= stamp();
	}
        force(&sys);
	if (rank == 0) {
	  timer += stamp();
	}
        velverlet_2(&sys);
#else
        /* write output, if requested */
	if (rank == 0) {
	  if ((sys.nfi % nprint) == 0)
            output(&sys, erg, traj);
	}
        velverlet_1(&sys);
        force(&sys);
        velverlet_2(&sys);
#endif
        ekin(&sys);
    }
    /**************************************************/

    /* clean up: close files, free memory */
    if (rank == 0) {
#ifdef TIMING
    printf("\n timing \n\t %f ms \n\n",timer);
#else
    printf("Simulation Done.\n");
    fclose(erg);
    fclose(traj);
#endif
    }

    free(sys.rx);
    free(sys.ry);
    free(sys.rz);
    free(sys.vx);
    free(sys.vy);
    free(sys.vz);
    free(sys.fx);
    free(sys.fy);
    free(sys.fz);
 

    MPI_Finalize();

    return 0;
}
