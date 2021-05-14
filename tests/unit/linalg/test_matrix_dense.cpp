// Copyright (c) 2010-2021, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#include "mfem.hpp"
#include "unit_tests.hpp"
#include "linalg/dtensor.hpp"

using namespace mfem;

TEST_CASE("DenseMatrix init-list construction", "[DenseMatrix]")
{
   double ContigData[6] = {6.0, 5.0, 4.0, 3.0, 2.0, 1.0};
   DenseMatrix Contiguous(ContigData, 2, 3);

   DenseMatrix Nested(
   {
      {6.0, 4.0, 2.0},
      {5.0, 3.0, 1.0}
   });

   for (int i = 0; i < Contiguous.Height(); i++)
   {
      for (int j = 0; j < Contiguous.Width(); j++)
      {
         REQUIRE(Nested(i,j) == Contiguous(i,j));
      }
   }
}

TEST_CASE("DenseMatrix LinearSolve methods",
          "[DenseMatrix]")
{
   SECTION("singular_system")
   {
      constexpr int N = 3;

      DenseMatrix A(N);
      A.SetRow(0, 0.0);
      A.SetRow(1, 0.0);
      A.SetRow(2, 0.0);

      double X[3];

      REQUIRE_FALSE(LinearSolve(A,X));
   }

   SECTION("1x1_system")
   {
      constexpr int N = 1;
      DenseMatrix A(N);
      A(0,0) = 2;

      double X[1] = { 12 };

      REQUIRE(LinearSolve(A,X));
      REQUIRE(X[0] == MFEM_Approx(6));
   }

   SECTION("2x2_system")
   {
      constexpr int N = 2;

      DenseMatrix A(N);
      A(0,0) = 2.0; A(0,1) = 1.0;
      A(1,0) = 3.0; A(1,1) = 4.0;

      double X[2] = { 1, 14 };

      REQUIRE(LinearSolve(A,X));
      REQUIRE(X[0] == MFEM_Approx(-2));
      REQUIRE(X[1] == MFEM_Approx(5));
   }

   SECTION("3x3_system")
   {
      constexpr int N = 3;

      DenseMatrix A(N);
      A(0,0) = 4; A(0,1) =  5; A(0,2) = -2;
      A(1,0) = 7; A(1,1) = -1; A(1,2) =  2;
      A(2,0) = 3; A(2,1) =  1; A(2,2) =  4;

      double X[3] = { -14, 42, 28 };

      REQUIRE(LinearSolve(A,X));
      REQUIRE(X[0] == MFEM_Approx(4));
      REQUIRE(X[1] == MFEM_Approx(-4));
      REQUIRE(X[2] == MFEM_Approx(5));
   }

}

TEST_CASE("DenseMatrix A*B^T methods",
          "[DenseMatrix]")
{
   double tol = 1e-12;

   double AtData[6] = {6.0, 5.0,
                       4.0, 3.0,
                       2.0, 1.0
                      };
   double BtData[12] = {1.0, 3.0, 5.0, 7.0,
                        2.0, 4.0, 6.0, 8.0,
                        1.0, 2.0, 3.0, 5.0
                       };

   DenseMatrix A(AtData, 2, 3);
   DenseMatrix B(BtData, 4, 3);
   DenseMatrix C(2,4);

   SECTION("MultABt")
   {
      double BData[12] = {1.0, 2.0, 1.0,
                          3.0, 4.0, 2.0,
                          5.0, 6.0, 3.0,
                          7.0, 8.0, 5.0
                         };
      DenseMatrix Bt(BData, 3, 4);

      double CtData[8] = {16.0, 12.0,
                          38.0, 29.0,
                          60.0, 46.0,
                          84.0, 64.0
                         };
      DenseMatrix Cexact(CtData, 2, 4);

      MultABt(A, B, C);
      C.Add(-1.0, Cexact);

      REQUIRE(C.MaxMaxNorm() < tol);

      Mult(A, Bt, Cexact);
      MultABt(A, B, C);
      C.Add(-1.0, Cexact);

      REQUIRE(C.MaxMaxNorm() < tol);
   }
   SECTION("MultADBt")
   {
      double DData[3] = {11.0, 7.0, 5.0};
      Vector D(DData, 3);

      double CtData[8] = {132.0, 102.0,
                          330.0, 259.0,
                          528.0, 416.0,
                          736.0, 578.0
                         };
      DenseMatrix Cexact(CtData, 2, 4);

      MultADBt(A, D, B, C);
      C.Add(-1.0, Cexact);

      REQUIRE(C.MaxMaxNorm() < tol);
   }
   SECTION("AddMultABt")
   {
      double CtData[8] = {17.0, 17.0,
                          40.0, 35.0,
                          63.0, 53.0,
                          88.0, 72.0
                         };
      DenseMatrix Cexact(CtData, 2, 4);

      C(0, 0) = 1.0; C(0, 1) = 2.0; C(0, 2) = 3.0; C(0, 3) = 4.0;
      C(1, 0) = 5.0; C(1, 1) = 6.0; C(1, 2) = 7.0; C(1, 3) = 8.0;

      AddMultABt(A, B, C);
      C.Add(-1.0, Cexact);

      REQUIRE(C.MaxMaxNorm() < tol);

      MultABt(A, B, C);
      C *= -1.0;
      AddMultABt(A, B, C);
      REQUIRE(C.MaxMaxNorm() < tol);
   }
   SECTION("AddMultADBt")
   {
      double DData[3] = {11.0, 7.0, 5.0};
      Vector D(DData, 3);

      double CtData[8] = {133.0, 107.0,
                          332.0, 265.0,
                          531.0, 423.0,
                          740.0, 586.0
                         };
      DenseMatrix Cexact(CtData, 2, 4);

      C(0, 0) = 1.0; C(0, 1) = 2.0; C(0, 2) = 3.0; C(0, 3) = 4.0;
      C(1, 0) = 5.0; C(1, 1) = 6.0; C(1, 2) = 7.0; C(1, 3) = 8.0;

      AddMultADBt(A, D, B, C);
      C.Add(-1.0, Cexact);

      REQUIRE(C.MaxMaxNorm() < tol);

      MultADBt(A, D, B, C);
      C *= -1.0;
      AddMultADBt(A, D, B, C);
      REQUIRE(C.MaxMaxNorm() < tol);

      DData[0] = 1.0; DData[1] = 1.0; DData[2] = 1.0;
      MultABt(A, B, C);
      C *= -1.0;
      AddMultADBt(A, D, B, C);
      REQUIRE(C.MaxMaxNorm() < tol);
   }
   SECTION("AddMult_a_ABt")
   {
      double a = 3.0;

      double CtData[8] = { 49.0,  41.0,
                           116.0,  93.0,
                           183.0, 145.0,
                           256.0, 200.0
                         };
      DenseMatrix Cexact(CtData, 2, 4);

      C(0, 0) = 1.0; C(0, 1) = 2.0; C(0, 2) = 3.0; C(0, 3) = 4.0;
      C(1, 0) = 5.0; C(1, 1) = 6.0; C(1, 2) = 7.0; C(1, 3) = 8.0;

      AddMult_a_ABt(a, A, B, C);
      C.Add(-1.0, Cexact);

      REQUIRE(C.MaxMaxNorm() < tol);

      MultABt(A, B, C);
      AddMult_a_ABt(-1.0, A, B, C);

      REQUIRE(C.MaxMaxNorm() < tol);
   }
}

TEST_CASE("KronMult methods",
          "[DenseMatrix]")
{
   double tol = 1e-12;
   int nA = 3, mA = 4;
   int nB = 5, mB = 6;
   DenseMatrix A(nA,mA);
   DenseMatrix B(nB,mB);

   for (int i = 0; i<nA; i++)
      for (int j = 0; j<mA; j++)
      {
         A(i,j) = ((double)rand()/(double)RAND_MAX);
      }

   for (int i = 0; i<nB; i++)
      for (int j = 0; j<mB; j++)
      {
         B(i,j) = ((double)rand()/(double)RAND_MAX);
      }

   DenseMatrix AB;
   KronProd(A,B,AB);

   // (A ⊗ B) r
   SECTION("KronMultABr")
   {
      Vector r(mA*mB); r.Randomize();
      MFEM_VERIFY(r.Size() == AB.Width(), "Check r size");
      Vector z0(AB.Height());
      AB.Mult(r,z0);

      Vector z1;
      KronMult(A,B,r,z1);
      MFEM_VERIFY(z0.Size() == z1.Size(), "Check z1 size");
      z0-=z1;
      REQUIRE(z0.Norml2() < tol);
   }
   // (A ⊗ B) R
   SECTION("KronMultABR")
   {
      int nR = mA*mB;
      int mR = 7;
      DenseMatrix R(nR, mR);
      for (int i = 0; i<nR; i++)
         for (int j = 0; j<mR; j++)
         {
            R(i,j) = ((double)rand()/(double)RAND_MAX);
         }

      DenseMatrix Z0(nA*nB,mR);
      Mult(AB,R,Z0);

      DenseMatrix Z1;
      KronMult(A,B,R,Z1);
      MFEM_VERIFY(Z0.Height() == Z1.Height() &&
                  Z0.Width() == Z1.Width(), "Check z1 size");
      Z0-=Z1;

      REQUIRE(Z0.MaxMaxNorm() < tol);

   }

   // (A ⊗ B ⊗ C) r
   SECTION("KronMultABCr")
   {
      int nC = 7, mC = 2;
      DenseMatrix C(nC, mC);
      for (int i = 0; i<nC; i++)
         for (int j = 0; j<mC; j++)
         {
            C(i,j) = ((double)rand()/(double)RAND_MAX);
         }

      DenseMatrix ABC;
      KronProd(AB,C,ABC);
      Vector r(mA*mB*mC); r.Randomize();
      MFEM_VERIFY(r.Size() == ABC.Width(), "Check r size");
      Vector z0(nA*nB*nC);
      ABC.Mult(r,z0);

      Vector z1;
      KronMult(A,B,C,r,z1);
      MFEM_VERIFY(z0.Size() == z1.Size(), "Check z1 size");
      z0-=z1;
      REQUIRE(z0.Norml2() < tol);
   }
}

TEST_CASE("KronMultInv methods",
          "[DenseMatrixInverse]")
{
   double tol = 1e-12;
   int nA = 3;
   int nB = 2;
   DenseMatrix A(
   {
      { 1.0,  0.2, 3.4},
      {-2.0, -1.0, 3.1},
      { 0.7,  1.4,-0.9}
   });
   DenseMatrix B(
   {
      {-10.1, 5.7},
      {-3.0,  4.2}
   });

   DenseMatrixInverse Ainv(A);
   DenseMatrixInverse Binv(B);

   DenseMatrix AB;
   KronProd(A,B,AB);

   // (A^-1 ⊗ B^-1) r
   SECTION("KronMultInvABr")
   {

      Vector r(nA*nB); r.Randomize();
      MFEM_VERIFY(r.Size() == AB.Width(), "Check r size");
      Vector z0(AB.Height());
      DenseMatrixInverse ABinv(AB);
      ABinv.Mult(r,z0);

      Vector z1;
      KronMult(Ainv,Binv,r,z1);
      MFEM_VERIFY(z0.Size() == z1.Size(), "Check z1 size");
      z0-=z1;
      REQUIRE(z0.Norml2() < tol);
   }

   // (A^-1 ⊗ B^-1) R
   SECTION("KronMultInvABR")
   {
      int nR = nA*nB;
      int mR = 7;
      DenseMatrix R(nR, mR);
      for (int i = 0; i<nR; i++)
         for (int j = 0; j<mR; j++)
         {
            R(i,j) = ((double)rand()/(double)RAND_MAX);
         }

      DenseMatrixInverse ABinv(AB);
      DenseMatrix Z0(nA*nB,mR);
      ABinv.Mult(R,Z0);

      DenseMatrix Z1;
      KronMult(Ainv,Binv,R,Z1);
      MFEM_VERIFY(Z0.Height() == Z1.Height() &&
                  Z0.Width() == Z1.Width(), "Check z1 size");
      Z0-=Z1;

      REQUIRE(Z0.MaxMaxNorm() < tol);
   }

   // (A^-1 ⊗ B^-1 ⊗ C^-1) r
   SECTION("KronMultInvABCr")
   {
      int nC = 4;
      DenseMatrix C(
      {
         {-2.1, 1.6, -3.4,  17.5},
         {-7.1, 1.3, -7.5, -12.5},
         { 0.5, 5.7, -6.0, -0.5},
         { 9.2, 0.3, -1.4, -14.9}
      });
      DenseMatrix ABC;
      KronProd(AB,C,ABC);
      DenseMatrixInverse ABCInv(ABC);
      Vector r(nA*nB*nC); r.Randomize();
      MFEM_VERIFY(r.Size() == ABC.Width(), "Check r size");
      Vector z0(nA*nB*nC);
      ABCInv.Mult(r,z0);

      DenseMatrixInverse Cinv(C);
      Vector z1;
      KronMult(Ainv,Binv,Cinv,r,z1);
      MFEM_VERIFY(z0.Size() == z1.Size(), "Check z1 size");
      z0-=z1;
      REQUIRE(z0.Norml2() < tol);
   }
}

TEST_CASE("LUFactors RightSolve", "[DenseMatrix]")
{
   double tol = 1e-12;

   // Zero on diagonal forces non-trivial pivot
   double AData[9] = { 0.0, 0.0, 3.0, 2.0, 2.0, 2.0, 2.0, 0.0, 4.0 };
   double BData[6] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
   int ipiv[3];

   DenseMatrix A(AData, 3, 3);
   DenseMatrix B(BData, 2, 3);

   DenseMatrixInverse Af1(A);
   DenseMatrix Ainv;
   Af1.GetInverseMatrix(Ainv);

   LUFactors Af2(AData, ipiv);
   Af2.Factor(3);

   DenseMatrix C(2,3);
   Mult(B, Ainv, C);
   Af2.RightSolve(3, 2, B.GetData());
   C -= B;

   REQUIRE(C.MaxMaxNorm() < tol);
}

TEST_CASE("DenseTensor LinearSolve methods",
          "[DenseMatrix]")
{

   int N = 3;
   DenseMatrix A(N);
   A(0,0) = 4; A(0,1) =  5; A(0,2) = -2;
   A(1,0) = 7; A(1,1) = -1; A(1,2) =  2;
   A(2,0) = 3; A(2,1) =  1; A(2,2) =  4;

   double X[3] = { -14, 42, 28 };

   int NE = 10;
   Vector X_batch(N*NE);
   DenseTensor A_batch(N,N,NE);

   auto a_batch = mfem::Reshape(A_batch.HostWrite(),N,N,NE);
   auto x_batch = mfem::Reshape(X_batch.HostWrite(),N,NE);
   // Column major
   for (int e=0; e<NE; ++e)
   {

      for (int r=0; r<N; ++r)
      {
         for (int c=0; c<N; ++c)
         {
            a_batch(c, r, e) = A.GetData()[c+r*N];
         }
         x_batch(r,e) = X[r];
      }
   }

   Array<int> P;
   BatchLUFactor(A_batch, P);
   BatchLUSolve(A_batch, P, X_batch);

   auto xans_batch = mfem::Reshape(X_batch.HostRead(),N,NE);
   REQUIRE(LinearSolve(A,X));
   for (int e=0; e<NE; ++e)
   {
      for (int r=0; r<N; ++r)
      {
         REQUIRE(xans_batch(r,e) == MFEM_Approx(X[r]));
      }
   }
}

#ifdef MFEM_USE_LAPACK

TEST_CASE("EigenSystem methods",
          "[DenseMatrix]")
{
   double tol = 1e-12;
   SECTION("SPD Matrix")
   {
      DenseMatrix A({{0.56806, 0.29211, 0.48315, 0.70024},
         {0.29211, 0.85147, 0.68123, 0.70689},
         {0.48315, 0.68123, 1.07229, 1.02681},
         {0.70024, 0.70689, 1.02681, 1.15468}
      });
      DenseMatrix V, AV(4);
      Vector Lambda;
      for (bool sym: { false, true })
      {
         DenseMatrixEigensystem  EigA(A,sym);
         EigA.Eval();
         V = EigA.Eigenvectors();
         Lambda = EigA.Eigenvalues();
         Mult(A,V,AV);
         V.RightScaling(Lambda);
         AV -= V;
         REQUIRE(AV.MaxMaxNorm() < tol);
      }
   }

   SECTION("Indefinite Matrix")
   {
      DenseMatrix A({{0.486278, 0.041135, 0.480727, 0.616026},
         {0.523599, 0.119827, 0.087808, 0.415241},
         {0.214454, 0.661631, 0.909626, 0.744259},
         {0.107007, 0.630604, 0.077862, 0.221006}
      });
      DenseMatrixEigensystem  EigA(A);
      EigA.Eval();

      Vector Lambda_r, Lambda_i;
      // Real part of eigenvalues
      Lambda_r = EigA.Eigenvalues();
      // Imag part of eigenvalues
      Lambda_i = EigA.Eigenvalues(true);

      DenseMatrix V;
      V = EigA.Eigenvectors();
      // Real part of eigenvectors
      DenseMatrix Vr(4), Vi(4);
      Vr.SetCol(0,V.GetColumn(0));
      Vr.SetCol(1,V.GetColumn(1));
      Vr.SetCol(2,V.GetColumn(1));
      Vr.SetCol(3,V.GetColumn(3));

      // Imag part of eigenvectors
      Vector vi(4); V.GetColumn(2,vi);
      Vi.SetCol(0,0.);
      Vi.SetCol(1,vi); vi *= -1.;
      Vi.SetCol(2,vi);
      Vi.SetCol(3,0.);

      // Check that A*V = V * Lambda
      // or A * (V_r + i V_i ) = (V_r + i V_i)*(Lamda_r + i Lambda_i)
      // or  A * V_r = V_r * Lambda_r -  V_i * Lambda_i
      // and A * V_i = V_r ( Lambda_i +  V_i * Lambda_r
      DenseMatrix AVr(4), AVi(4);
      Mult(A,Vr, AVr);
      Mult(A,Vi, AVi);

      DenseMatrix Vrlr = Vr; Vrlr.RightScaling(Lambda_r);
      DenseMatrix Vrli = Vr; Vrli.RightScaling(Lambda_i);
      DenseMatrix Vilr = Vi; Vilr.RightScaling(Lambda_r);
      DenseMatrix Vili = Vi; Vili.RightScaling(Lambda_i);

      AVr -= Vrlr; AVr+= Vili;
      AVi -= Vrli; AVi-= Vilr;

      REQUIRE(AVr.MaxMaxNorm() < tol);
      REQUIRE(AVi.MaxMaxNorm() < tol);
   }
}

#endif // if MFEM_USE_LAPACK
