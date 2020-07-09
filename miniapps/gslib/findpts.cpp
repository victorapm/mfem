﻿// Copyright (c) 2010-2020, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.
//
//      -------------------------------------------------------------
//      Find Points Miniapp: Evaluate grid function in physical space
//      -------------------------------------------------------------
//
// This miniapp demonstrates the interpolation of a high-order grid function on
// a set of points in physical-space. The miniapp is based on GSLIB-FindPoints,
// which provides two key functionalities. First, for a given set of points in
// the physical-space, it determines the computational coordinates (element
// number, reference-space coordinates inside the element, and processor number
// [in parallel]) for each point. Second, based on computational coordinates, it
// interpolates a grid function in the given points. Inside GSLIB, computation
// of the coordinates requires use of a Hash Table to identify the candidate
// processor and element for each point, followed by the Newton's method to
// determine the reference-space coordinates inside the candidate element.
//
// Compile with: make findpts
//
// Sample runs:
//    findpts -m ../../data/rt-2d-q3.mesh -o 3 -mo 3 -ft 2
//    findpts -m ../../data/rt-2d-p4-tri.mesh -o 4
//    findpts -m ../../data/inline-tri.mesh -o 3
//    findpts -m ../../data/inline-quad.mesh -o 3
//    findpts -m ../../data/inline-tet.mesh -o 3
//    findpts -m ../../data/inline-hex.mesh -o 3
//    findpts -m ../../data/inline-wedge.mesh -o 3
//    findpts -m ../../data/amr-quad.mesh -o 2

#include "mfem.hpp"

using namespace mfem;
using namespace std;

// Scalar function to project
double field_func(const Vector &x)
{
   const int dim = x.Size();
   double res = 0.0;
   for (int d = 0; d < dim; d++) { res += x(d) * x(d); }
   return res;
}

void F_exact(const Vector &p, Vector &F)
{
   F(0) = field_func(p);
   for (int i = 1; i < F.Size(); i++) { F(i) = (i+1)*F(0); }
}

int main (int argc, char *argv[])
{
   // Set the method's default parameters.
   const char *mesh_file = "../../data/rt-2d-q3.mesh";
   int order             = 3;
   int mesh_poly_deg     = 3;
   int rs_levels         = 0;
   bool visualization    = true;
   int fieldtype         = 0;
   int ncomp             = 1;

   // Parse command-line options.
   OptionsParser args(argc, argv);
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Mesh file to use.");
   args.AddOption(&order, "-o", "--order",
                  "Finite element order (polynomial degree).");
   args.AddOption(&mesh_poly_deg, "-mo", "--mesh-order",
                  "Polynomial degree of mesh finite element space.");
   args.AddOption(&rs_levels, "-rs", "--refine-serial",
                  "Number of times to refine the mesh uniformly in serial.");
   args.AddOption(&fieldtype, "-ft", "--field-type",
                  "Field type: 0 - H1, 1 - L2, 2 - H(div), 3 - H(curl).");
   args.AddOption(&ncomp, "-nc", "--ncomp",
                  "VDim for GridFunction");
   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",
                  "--no-visualization",
                  "Enable or disable GLVis visualization.");
   args.Parse();
   if (!args.Good())
   {
      args.PrintUsage(cout);
      return 1;
   }
   args.PrintOptions(cout);

   // Initialize and refine the starting mesh.
   Mesh mesh(mesh_file, 1, 1, false);
   for (int lev = 0; lev < rs_levels; lev++) { mesh.UniformRefinement(); }
   const int dim = mesh.Dimension();
   cout << "Mesh curvature of the original mesh: ";
   if (mesh.GetNodes()) { cout << mesh.GetNodes()->OwnFEC()->Name(); }
   else { cout << "(NONE)"; }
   cout << endl;

   // Mesh bounding box.
   Vector pos_min, pos_max;
   MFEM_VERIFY(mesh_poly_deg > 0, "The order of the mesh must be positive.");
   mesh.GetBoundingBox(pos_min, pos_max, mesh_poly_deg);
   cout << "--- Generating equidistant point for:\n"
        << "x in [" << pos_min(0) << ", " << pos_max(0) << "]\n"
        << "y in [" << pos_min(1) << ", " << pos_max(1) << "]\n";
   if (dim == 3)
   {
      cout << "z in [" << pos_min(2) << ", " << pos_max(2) << "]\n";
   }

   // Curve the mesh based on the chosen polynomial degree.
   H1_FECollection fec(mesh_poly_deg, dim);
   FiniteElementSpace fespace(&mesh, &fec, dim);
   mesh.SetNodalFESpace(&fespace);
   cout << "Mesh curvature of the curved mesh: " << fec.Name() << endl;

   MFEM_ASSERT(ncomp > 0, " Invalid input for ncomp.");
   int ncfinal = ncomp;
   GridFunction field_vals;
   H1_FECollection fech(order, dim);
   L2_FECollection fecl(order, dim);
   ND_FECollection fechdiv(order, dim);
   RT_FECollection feccurl(order, dim);
   FiniteElementSpace *sc_fes = NULL;
   if (fieldtype == 0)
   {
      sc_fes = new FiniteElementSpace(&mesh, &fech, ncomp);
      cout << "H1-GridFunction\n";
   }
   else if (fieldtype == 1)
   {
      sc_fes = new FiniteElementSpace(&mesh, &fecl, ncomp);
      cout << "L2-GridFunction\n";
   }
   else if (fieldtype == 2)
   {
      sc_fes = new FiniteElementSpace(&mesh, &fechdiv);
      ncfinal = dim;
      cout << "H(div)-GridFunction\n";
   }
   else if (fieldtype == 3)
   {
      sc_fes = new FiniteElementSpace(&mesh, &feccurl);
      ncfinal = dim;
      cout << "H(curl)-GridFunction\n";
   }
   field_vals.SetSpace(sc_fes);

   // Project the GridFunction using VectorFunctionCoefficient.
   VectorFunctionCoefficient F(ncfinal, F_exact);
   field_vals.ProjectCoefficient(F);

   // Display the mesh and the field through glvis.
   if (visualization)
   {
      char vishost[] = "localhost";
      int  visport   = 19916;
      socketstream sout;
      sout.open(vishost, visport);
      if (!sout)
      {
         cout << "Unable to connect to GLVis server at "
              << vishost << ':' << visport << endl;
      }
      else
      {
         sout.precision(8);
         sout << "solution\n" << mesh << field_vals;
         if (dim == 2) { sout << "keys RmjA*****\n"; }
         if (dim == 3) { sout << "keys mA\n"; }
         sout << flush;
      }
   }

   // Generate equidistant points in physical coordinates over the whole mesh.
   // Note that some points might be outside, if the mesh is not a box. Note
   // also that all tasks search the same points (not mandatory).
   const int pts_cnt_1D = 5;
   const int pts_cnt = pow(pts_cnt_1D, dim);
   Vector vxyz(pts_cnt * dim);
   if (dim == 2)
   {
      L2_QuadrilateralElement el(pts_cnt_1D - 1, BasisType::ClosedUniform);
      const IntegrationRule &ir = el.GetNodes();
      for (int i = 0; i < ir.GetNPoints(); i++)
      {
         const IntegrationPoint &ip = ir.IntPoint(i);
         vxyz(i)           = pos_min(0) + ip.x * (pos_max(0)-pos_min(0));
         vxyz(pts_cnt + i) = pos_min(1) + ip.y * (pos_max(1)-pos_min(1));
      }
   }
   else
   {
      L2_HexahedronElement el(pts_cnt_1D - 1, BasisType::ClosedUniform);
      const IntegrationRule &ir = el.GetNodes();
      for (int i = 0; i < ir.GetNPoints(); i++)
      {
         const IntegrationPoint &ip = ir.IntPoint(i);
         vxyz(i)             = pos_min(0) + ip.x * (pos_max(0)-pos_min(0));
         vxyz(pts_cnt + i)   = pos_min(1) + ip.y * (pos_max(1)-pos_min(1));
         vxyz(2*pts_cnt + i) = pos_min(2) + ip.z * (pos_max(2)-pos_min(2));
      }
   }

   // Find and Interpolate FE function values on the desired points.
   Vector interp_vals(pts_cnt*ncfinal);
   FindPointsGSLIB finder;
   finder.Setup(mesh);
   finder.Interpolate(vxyz, field_vals, interp_vals);
   Array<unsigned int> code_out    = finder.GetCode();
   Array<unsigned int> task_id_out = finder.GetProc();
   Vector dist_p_out = finder.GetDist();

   int face_pts = 0, not_found = 0, found = 0;
   double max_err = 0.0, max_dist = 0.0;
   Vector pos(dim);
   int npt = 0;
   for (int j = 0; j < ncfinal; j++)
   {
      for (int i = 0; i < pts_cnt; i++)
      {
         if (code_out[i] < 2)
         {
            if (j == 0) { found++; }
            for (int d = 0; d < dim; d++) { pos(d) = vxyz(d * pts_cnt + i); }
            Vector exact_val(ncfinal);
            F_exact(pos, exact_val);
            max_err  = std::max(max_err, fabs(exact_val(j) - interp_vals[npt]));
            max_dist = std::max(max_dist, dist_p_out(i));
            if (code_out[i] == 1 && j == 0) { face_pts++; }
         }
         else { if (j == 0) { not_found++; } }
         npt++;
      }
   }

   cout << setprecision(16)
        << "Searched points:     "   << pts_cnt
        << "\nFound points:        " << found
        << "\nMax interp error:    " << max_err
        << "\nMax dist (of found): " << max_dist
        << "\nPoints not found:    " << not_found
        << "\nPoints on faces:     " << face_pts << endl;

   // Free the internal gslib data.
   finder.FreeData();
   return 0;
}
