// Copyright (C) 2009 by Thomas Moulard, FIXME.
//
// This file is part of the liboptimization.
//
// liboptimization is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// liboptimization is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with liboptimization.  If not, see <http://www.gnu.org/licenses/>.


/**
 * \file src/cfsqp.cc
 *
 * \brief Implementation of the CFSQP module.
 */
#include <limits>
#include <cfsqpusr.h>

#include "cfsqp.hh"
#include "function.hh"
#include "problem.hh"
#include "util.hh"

namespace optimization
{
  typedef void (*fct_t) (int, int, double*, double*, void*);
  typedef void (*grad_t) (int, int, double*, double*, fct_t, void*);

  namespace detail
  {
    /// CFSQP objective function.
    void obj (int nparam, int j , double* x, double* fj, void* cd)
    {
      assert (cd);
      CFSQPSolver* solver = static_cast<CFSQPSolver*> (cd);

      Function::vector_t x_ (nparam);
      array_to_vector (x_, x);
      *fj = solver->problem.function (x_);
    }

    /// CFSQP constraints function.
    void constr (int nparam, int j,
                 double* x, double* gj, void* cd)
    {
      assert (cd);
      CFSQPSolver* solver = static_cast<CFSQPSolver*> (cd);

      Function::vector_t x_ (nparam);
      array_to_vector (x_, x);

      int j_ = (j < 2) ? 0 : (j - 1)/2;
      *gj = (*solver->problem.constraints [j_]) (x_);

      if (j % 2 == 0)
        // g(x) >= b, -g(x) + b <= 0
        *gj = -*gj + solver->problem.constraints [j_]->bound.first;
      else
        // g(x) <= b, g(x) - b <= 0
        *gj = *gj - solver->problem.constraints [j_]->bound.second;
    }

    /// CFSQP objective function gradient.
    void gradob (int nparam, int j,
                 double* x, double* gradf, fct_t dummy, void* cd)
    {
      assert (cd);
      assert (j == 1);

      CFSQPSolver* solver = static_cast<CFSQPSolver*> (cd);

      Function::vector_t x_ (nparam);
      array_to_vector (x_, x);
      Function::gradient_t grad = solver->problem.function.gradient (x_);

      if (!grad)
        ::grobfd (nparam, j, x, gradf, dummy, cd);
      *gradf = (*grad)[j]; //FIXME: check that.
    }

    /// CFSQP constraints function gradient.
    void gradcn (int nparam, int j,
                 double* x, double* gradgj, fct_t dummy, void* cd)
    {
      assert (cd);
      //CFSQPSolver* solver = static_cast<CFSQPSolver*> (cd);

      //FIXME: if gradient of constraints available, use it else...
      //if (!solver.getGradient ())
      ::grcnfd (nparam, j, x, gradgj, dummy, cd);
    }

  }

  CFSQPSolver::CFSQPSolver (const Problem& pb) throw ()
    : Solver (pb)
  {
  }

  CFSQPSolver::~CFSQPSolver () throw ()
  {
  }

  void
  CFSQPSolver::initialize_bounds (double* bl, double* bu) const throw ()
  {
    typedef Function::bounds_t::const_iterator citer_t;

    Function::size_type i = 0;
    for (citer_t it = problem.function.argBounds.begin ();
         it != problem.function.argBounds.end (); ++it)
      {
        bl[i] = (*it).first, bu[i] = (*it).second;
        ++i;
      }
  }

  CFSQPSolver::result_t
  CFSQPSolver::getMinimum () throw ()
  {
    if (result_.which () != SOLVER_NO_SOLUTION)
      return result_;

    int nparam = problem.function.n;
    int nf = 1; //FIXME: only one objective function.
    int nfsr = 0;
    int nineqn = 2 * problem.constraints.size ();
    int nineq = 2 * problem.constraints.size ();
    int neqn = 0;
    int neq = 0;
    int ncsrl = 0;
    int ncsrn = 0;
    int mesh_pts[1];
    int mode = 100;
    int iprint = 0;
    int miter = 500;
    int inform = 0;
    double bignd = std::numeric_limits<Function::value_type>::infinity ();
    double eps = 1e-8;
    double epseqn = 1e-8;
    double udelta = 1e-8;
    double bl[nparam];
    double bu[nparam];
    double x[nparam];
    double f[1];
    double g[2 * problem.constraints.size ()];
    double lambda[nparam + 1 + 2 * problem.constraints.size ()];
    fct_t obj = detail::obj;
    fct_t constr = detail::constr;
    grad_t gradob = detail::gradob;
    grad_t gradcn = detail::gradcn;
    void* cd = this;

    // Initialize bounds.
    initialize_bounds (bl, bu);

    // Copy starting point.
    if (!problem.start.empty ())
      memcpy (x, &problem.start[0], nparam * sizeof (Function::value_type));


    cfsqp (nparam, nf, nfsr, nineqn, nineq, neqn, neq, ncsrl,  ncsrn,
           mesh_pts, mode,  iprint, miter, &inform, bignd, eps, epseqn,
           udelta, bl, bu, x, f, g, lambda,
           obj, constr, gradob, gradcn, cd);

    if (inform == 0)
      {
        Function::vector_t x_ (nparam);
        detail::array_to_vector (x_, x);
        result_ = x_;
      }
    else
      result_ = SolverError ("CFSQP has failed.");

    return result_;
  }

} // end of namespace optimization