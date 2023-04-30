//                       MFEM Example 2 - Parallel Version
//
// Compile with: make ex2p
//
// Sample runs:  mpirun -np 4 ex2p -m ../data/beam-tri.mesh
//               mpirun -np 4 ex2p -m ../data/beam-quad.mesh
//               mpirun -np 4 ex2p -m ../data/beam-tet.mesh
//               mpirun -np 4 ex2p -m ../data/beam-hex.mesh
//               mpirun -np 4 ex2p -m ../data/beam-wedge.mesh
//               mpirun -np 4 ex2p -m ../data/beam-tri.mesh -o 2 -sys
//               mpirun -np 4 ex2p -m ../data/beam-quad.mesh -o 3 -elast
//               mpirun -np 4 ex2p -m ../data/beam-quad.mesh -o 3 -sc
//               mpirun -np 4 ex2p -m ../data/beam-quad-nurbs.mesh
//               mpirun -np 4 ex2p -m ../data/beam-hex-nurbs.mesh
//
// Description:  This example code solves a simple linear elasticity problem
//               describing a multi-material cantilever beam.
//
//               Specifically, we approximate the weak form of -div(sigma(u))=0
//               where sigma(u)=lambda*div(u)*I+mu*(grad*u+u*grad) is the stress
//               tensor corresponding to displacement field u, and lambda and mu
//               are the material Lame constants. The boundary conditions are
//               u=0 on the fixed part of the boundary with attribute 1, and
//               sigma(u).n=f on the remainder with f being a constant pull down
//               vector on boundary elements with attribute 2, and zero
//               otherwise. The geometry of the domain is assumed to be as
//               follows:
//
//                                 +----------+----------+
//                    boundary --->| material | material |<--- boundary
//                    attribute 1  |    1     |    2     |     attribute 2
//                    (fixed)      +----------+----------+     (pull down)
//
//               The example demonstrates the use of high-order and NURBS vector
//               finite element spaces with the linear elasticity bilinear form,
//               meshes with curved elements, and the definition of piece-wise
//               constant and vector coefficient objects. Static condensation is
//               also illustrated.
//
//               We recommend viewing Example 1 before viewing this example.

#include "mfem.hpp"
#include <fstream>
#include <iostream>

using namespace std;
using namespace mfem;

int main(int argc, char *argv[])
{
   // 1. Initialize MPI and HYPRE.
   Mpi::Init(argc, argv);
   int num_procs = Mpi::WorldSize();
   int myid = Mpi::WorldRank();
   Hypre::Init();

   // 2. Parse command-line options.
   const char *mesh_file = "../data/beam-tri.mesh";
   int prec_print_level = 1;
   int order = 1;
   int serial_ref_levels = 4;
   int parallel_ref_levels = 1;
   int num_reps = 10;
   bool static_cond = false;
   bool visualization = true;
   bool amg_elast = false;
   bool amg_fsai = false;
   bool reorder_space = false;
   bool save_results = false;
   double amg_theta = -1.0;
   int amg_relax_type = -1;
   int amg_agg_num_levels = -1;
   int smooth_num_levels = -1;
   int fsai_num_levels = -1;
   int fsai_max_nnz_row = -1;
   int fsai_eig_max_iters = -1;
   double fsai_threshold = -1.0;

   const char *device_config = "cpu";

   OptionsParser args(argc, argv);
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Mesh file to use.");
   args.AddOption(&order, "-o", "--order",
                  "Finite element order (polynomial degree).");
   args.AddOption(&serial_ref_levels, "-sr", "--serial-ref",
                  "Number of refinement levels in serial.");
   args.AddOption(&parallel_ref_levels, "-pr", "--parallel-ref",
                  "Number of refinement levels in parallel.");
   args.AddOption(&num_reps, "-nr", "--num-reps",
                  "Number of repetitions for the linear solver Setup/Solve phases.");
   args.AddOption(&prec_print_level, "-ppl", "--prec-print-level",
                  "Hypre's preconditioner print level.");
   args.AddOption(&smooth_num_levels, "-smlv", "--smooth-num-levels",
                  "Number of levels of FSAI smoothing.");
   args.AddOption(&fsai_num_levels, "-fslv", "--fsai-num-levels",
                  "Number of levels for computing the candidate pattern for FSAI.");
   args.AddOption(&fsai_max_nnz_row, "-fsnnz", "--fsai-max-nnz-row",
                  "Maximum number of nonzero entries per row for FSAI.");
   args.AddOption(&fsai_eig_max_iters, "-fseig", "--fsai-eig-max-iters",
                  "Number of iterations for computing max. eigenvalue of FSAI.");
   args.AddOption(&fsai_threshold, "-fsth", "--fsai-threshold",
                  "Threshold for filtering candidate pattern of FSAI.");
   args.AddOption(&amg_theta, "-amgth", "--amg-theta",
                  "Threshold for defining strong coupling during AMG coarsening.");
   args.AddOption(&amg_agg_num_levels, "-amgagg", "--amg-agg-num-levels",
                  "Number of aggressive coarsening levels.");
   args.AddOption(&amg_relax_type, "-amgrt", "--amg-relax-type",
                  "Relaxation type for BoomerAMG.");
   args.AddOption(&amg_fsai, "-fsai", "--amg-fsai", "-no-fsai", "--no-amg-fsai",
                  "Use FSAI as a complex smoother to BoomerAMG");
   args.AddOption(&amg_elast, "-elast", "--amg-for-elasticity", "-sys",
                  "--amg-for-systems",
                  "Use the special AMG elasticity solver (GM/LN approaches), "
                  "or standard AMG for systems (unknown approach).");
   args.AddOption(&static_cond, "-sc", "--static-condensation", "-no-sc",
                  "--no-static-condensation", "Enable static condensation.");
   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",
                  "--no-visualization",
                  "Enable or disable GLVis visualization.");
   args.AddOption(&save_results, "-save", "--save-results", "-no-save", "--no-save-results",
                  "Save mesh and solution files.");
   args.AddOption(&reorder_space, "-nodes", "--by-nodes", "-vdim", "--by-vdim",
                  "Use byNODES ordering of vector space instead of byVDIM");
   args.AddOption(&device_config, "-d", "--device",
                  "Device configuration string, see Device::Configure().");
   args.Parse();
   if (!args.Good())
   {
      if (myid == 0)
      {
         args.PrintUsage(cout);
      }
      return 1;
   }
   if (myid == 0)
   {
      args.PrintOptions(cout);
   }

   // 3. Enable hardware devices such as GPUs, and programming models such as
   //    CUDA, OCCA, RAJA and OpenMP based on command line options.
   Device device(device_config);
   if (myid == 0) { device.Print(); }

   // 4. Read the (serial) mesh from the given mesh file on all processors.  We
   //    can handle triangular, quadrilateral, tetrahedral, hexahedral, surface
   //    and volume meshes with the same code.
   Mesh *mesh = new Mesh(mesh_file, 1, 1);
   int dim = mesh->Dimension();

   if (mesh->attributes.Max() < 2 || mesh->bdr_attributes.Max() < 2)
   {
      if (myid == 0)
         cerr << "\nInput mesh should have at least two materials and "
              << "two boundary attributes! (See schematic in ex2.cpp)\n"
              << endl;
      return 3;
   }

   // 5. Select the order of the finite element discretization space. For NURBS
   //    meshes, we increase the order by degree elevation.
   if (mesh->NURBSext)
   {
      mesh->DegreeElevate(order, order);
   }

   // 6. Refine the serial mesh on all processors to increase the resolution. In
   //    this example we do 'serial_ref_levels' of uniform refinement.
   {
      for (int l = 0; l < serial_ref_levels; l++)
      {
         mesh->UniformRefinement();
      }
   }

   // 7. Define a parallel mesh by a partitioning of the serial mesh. Refine
   //    this mesh further in parallel to increase the resolution. Once the
   //    parallel mesh is defined, the serial mesh can be deleted.
   ParMesh *pmesh = new ParMesh(MPI_COMM_WORLD, *mesh);
   delete mesh;
   {
      for (int l = 0; l < parallel_ref_levels; l++)
      {
         pmesh->UniformRefinement();
      }
   }

   // 8. Define a parallel finite element space on the parallel mesh. Here we
   //    use vector finite elements, i.e. dim copies of a scalar finite element
   //    space. We use the ordering by vector dimension (the last argument of
   //    the FiniteElementSpace constructor) which is expected in the systems
   //    version of BoomerAMG preconditioner. For NURBS meshes, we use the
   //    (degree elevated) NURBS space associated with the mesh nodes.
   FiniteElementCollection *fec;
   ParFiniteElementSpace *fespace;
   const bool use_nodal_fespace = pmesh->NURBSext && !amg_elast;
   if (use_nodal_fespace)
   {
      fec = NULL;
      fespace = (ParFiniteElementSpace *)pmesh->GetNodes()->FESpace();
   }
   else
   {
      fec = new H1_FECollection(order, dim);
      if (reorder_space)
      {
         fespace = new ParFiniteElementSpace(pmesh, fec, dim, Ordering::byNODES);
      }
      else
      {
         fespace = new ParFiniteElementSpace(pmesh, fec, dim, Ordering::byVDIM);
      }
   }
   HYPRE_BigInt size = fespace->GlobalTrueVSize();
   if (myid == 0)
   {
      cout << "Number of finite element unknowns: " << size << endl
           << "Assembling: " << flush;
   }

   // 9. Determine the list of true (i.e. parallel conforming) essential
   //    boundary dofs. In this example, the boundary conditions are defined by
   //    marking only boundary attribute 1 from the mesh as essential and
   //    converting it to a list of true dofs.
   Array<int> ess_tdof_list, ess_bdr(pmesh->bdr_attributes.Max());
   ess_bdr = 0;
   ess_bdr[0] = 1;
   fespace->GetEssentialTrueDofs(ess_bdr, ess_tdof_list);

   // 10. Set up the parallel linear form b(.) which corresponds to the
   //     right-hand side of the FEM linear system. In this case, b_i equals the
   //     boundary integral of f*phi_i where f represents a "pull down" force on
   //     the Neumann part of the boundary and phi_i are the basis functions in
   //     the finite element fespace. The force is defined by the object f, which
   //     is a vector of Coefficient objects. The fact that f is non-zero on
   //     boundary attribute 2 is indicated by the use of piece-wise constants
   //     coefficient for its last component.
   VectorArrayCoefficient f(dim);
   for (int i = 0; i < dim-1; i++)
   {
      f.Set(i, new ConstantCoefficient(0.0));
   }
   {
      Vector pull_force(pmesh->bdr_attributes.Max());
      pull_force = 0.0;
      pull_force(1) = -1.0e-2;
      f.Set(dim-1, new PWConstCoefficient(pull_force));
   }

   ParLinearForm *b = new ParLinearForm(fespace);
   b->AddBoundaryIntegrator(new VectorBoundaryLFIntegrator(f));
   if (myid == 0)
   {
      cout << "r.h.s. ... " << flush;
   }
   b->Assemble();

   // 11. Define the solution vector x as a parallel finite element grid
   //     function corresponding to fespace. Initialize x with initial guess of
   //     zero, which satisfies the boundary conditions.
   ParGridFunction x(fespace);
   x = 0.0;

   // 12. Set up the parallel bilinear form a(.,.) on the finite element space
   //     corresponding to the linear elasticity integrator with piece-wise
   //     constants coefficient lambda and mu.
   Vector lambda(pmesh->attributes.Max());
   lambda = 1.0;
   lambda(0) = lambda(1)*50;
   PWConstCoefficient lambda_func(lambda);
   Vector mu(pmesh->attributes.Max());
   mu = 1.0;
   mu(0) = mu(1)*50;
   PWConstCoefficient mu_func(mu);

   ParBilinearForm *a = new ParBilinearForm(fespace);
   a->AddDomainIntegrator(new ElasticityIntegrator(lambda_func, mu_func));

   // 13. Assemble the parallel bilinear form and the corresponding linear
   //     system, applying any necessary transformations such as: parallel
   //     assembly, eliminating boundary conditions, applying conforming
   //     constraints for non-conforming AMR, static condensation, etc.
   if (myid == 0) { cout << "matrix ... " << flush; }
   if (static_cond) { a->EnableStaticCondensation(); }
   a->Assemble();

   HypreParMatrix A;
   Vector B, X;
   a->FormLinearSystem(ess_tdof_list, x, *b, A, X, B);
   if (myid == 0)
   {
      cout << "done." << endl;
      cout << "Size of linear system: " << A.GetGlobalNumRows() << endl;
   }

   // 14. Define and apply a parallel PCG solver for A X = B with the BoomerAMG
   //     preconditioner from hypre.
   for (int i = 0; i < num_reps; i++)
   {
      if (!myid)
      {
         cout << endl;
         cout << "=============================================" << endl;
         cout << "Pass #" << i << "..." << endl;
         cout << "=============================================" << endl;
         cout << endl;
      }

      HypreBoomerAMG *amg = new HypreBoomerAMG(A);

      if (amg_elast && !a->StaticCondensationIsEnabled())
      {
         amg->SetElasticityOptions(fespace);
      }
      else
      {
         amg->SetSystemsOptions(dim, reorder_space);
      }

      if (amg_fsai)
      {
         amg->SetBoomerAMGFSAIOptions(prec_print_level, smooth_num_levels, fsai_num_levels,
                                      fsai_max_nnz_row, fsai_eig_max_iters, fsai_threshold);
      }
      amg->SetBoomerAMGPrintLevel(prec_print_level);
      amg->SetBoomerAMGStrongThreshold(amg_theta);
      amg->SetBoomerAMGRelaxType(amg_relax_type);
      amg->SetBoomerAMGAggNumLevels(amg_agg_num_levels);

      HyprePCG *pcg = new HyprePCG(A);
      pcg->SetTol(1e-8);
      pcg->SetMaxIter(500);
      pcg->SetPrintLevel(2);
      pcg->SetPreconditioner(*amg);
      pcg->Mult(B, X);

      if (i < (num_reps - 1)) X = 0.0;

      delete amg;
      delete pcg;

      /* Print Umpire statistics */
#if defined (HYPRE_USING_UMPIRE)
      {
         hypre_Handle *handle = hypre_handle();
         const char *pool_name = hypre_HandleUmpireDevicePoolName(handle);
         umpire_resourcemanager *rm_ptr = &hypre_HandleUmpireResourceMan(handle);
         umpire_allocator pooled_allocator;

         if ( umpire_resourcemanager_is_allocator_name(rm_ptr, pool_name) )
         {
            umpire_resourcemanager_get_allocator_by_name(rm_ptr, pool_name, &pooled_allocator);
            if (!myid)
            {
              cout << "[0]:       Pool name: "
                   << pool_name << endl
                   << "    Actual size (GB): "
                   << (double) umpire_allocator_get_actual_size(&pooled_allocator) / 1e9 << endl
                   << "   Current size (GB): "
                   << (double) umpire_allocator_get_current_size(&pooled_allocator) / 1e9 << endl
                   << " High watermark (GB): "
                   << (double) umpire_allocator_get_high_watermark(&pooled_allocator) / 1e9
                   << endl;
            }
         }
      }
#endif
   }

   // Additional run to print statistics (not counted for performance)
   if (prec_print_level == 0)
   {
      HypreBoomerAMG *amg = new HypreBoomerAMG(A);

      if (amg_elast && !a->StaticCondensationIsEnabled())
      {
         amg->SetElasticityOptions(fespace);
      }
      else
      {
         amg->SetSystemsOptions(dim, reorder_space);
      }

      if (amg_fsai)
      {
         amg->SetBoomerAMGFSAIOptions(1, smooth_num_levels, fsai_num_levels,
                                      fsai_max_nnz_row, fsai_eig_max_iters, fsai_threshold);
      }
      amg->SetBoomerAMGPrintLevel(1);
      amg->SetBoomerAMGStrongThreshold(amg_theta);
      amg->SetBoomerAMGRelaxType(amg_relax_type);
      amg->SetBoomerAMGAggNumLevels(amg_agg_num_levels);

      HyprePCG *pcg = new HyprePCG(A);
      pcg->SetTol(1e-8);
      pcg->SetMaxIter(500);
      pcg->SetPrintLevel(0);
      pcg->SetPreconditioner(*amg);
      pcg->Mult(B, X);

      delete amg;
      delete pcg;
   }

   // 15. Recover the parallel grid function corresponding to X. This is the
   //     local finite element solution on each processor.
   a->RecoverFEMSolution(X, *b, x);

   // 16. For non-NURBS meshes, make the mesh curved based on the finite element
   //     space. This means that we define the mesh elements through a fespace
   //     based transformation of the reference element.  This allows us to save
   //     the displaced mesh as a curved mesh when using high-order finite
   //     element displacement field. We assume that the initial mesh (read from
   //     the file) is not higher order curved mesh compared to the chosen FE
   //     space.
   if (!use_nodal_fespace)
   {
      pmesh->SetNodalFESpace(fespace);
   }

   // 17. Save in parallel the displaced mesh and the inverted solution (which
   //     gives the backward displacements to the original grid). This output
   //     can be viewed later using GLVis: "glvis -np <np> -m mesh -g sol".
   if (save_results)
   {
      GridFunction *nodes = pmesh->GetNodes();
      *nodes += x;
      x *= -1;

      ostringstream mesh_name, sol_name;
      mesh_name << "mesh." << setfill('0') << setw(6) << myid;
      sol_name << "sol." << setfill('0') << setw(6) << myid;

      ofstream mesh_ofs(mesh_name.str().c_str());
      mesh_ofs.precision(8);
      pmesh->Print(mesh_ofs);

      ofstream sol_ofs(sol_name.str().c_str());
      sol_ofs.precision(8);
      x.Save(sol_ofs);
   }

   // 18. Send the above data by socket to a GLVis server.  Use the "n" and "b"
   //     keys in GLVis to visualize the displacements.
   if (visualization)
   {
      char vishost[] = "localhost";
      int  visport   = 19916;
      socketstream sol_sock(vishost, visport);
      sol_sock << "parallel " << num_procs << " " << myid << "\n";
      sol_sock.precision(8);
      sol_sock << "solution\n" << *pmesh << x << flush;
   }

   // 19. Free the used memory.
   delete a;
   delete b;
   if (fec)
   {
      delete fespace;
      delete fec;
   }
   delete pmesh;

   return 0;
}
