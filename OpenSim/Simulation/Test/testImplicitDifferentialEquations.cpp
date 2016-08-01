/* -------------------------------------------------------------------------- *
 *              OpenSim:  testImplicitDifferentialEquations.cpp               *
 * -------------------------------------------------------------------------- *
 * The OpenSim API is a toolkit for musculoskeletal modeling and simulation.  *
 * See http://opensim.stanford.edu and the NOTICE file for more information.  *
 * OpenSim is developed at Stanford University and supported by the US        *
 * National Institutes of Health (U54 GM072970, R24 HD065690) and by DARPA    *
 * through the Warrior Web program.                                           *
 *                                                                            *
 * Copyright (c) 2016 Stanford University and the Authors                     *
 * Author(s): Chris Dembia, Brad Humphreys                                    *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.         *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

/** TODO
*/

// Tests:
// TODO component w/multiple state vars, only some of which have implicit form.
// TODO model containing components with and without explicit form.
// TODO write custom implicit form solver.
// TODO copying and (de)serializing models with implicit forms.
// TODO model containing multiple components with implicit form.
// TODO editing yDotGuess or lambdaGuess invalidates the residual cache.
// TODO calculation of YIndex must be correct.
// TODO test implicit form with multibody system but no auxiliary dynamics, as
//      well as only explicit auxiliary dynamics.
// TODO test given lambda.
// TODO debug by comparing directly to result of calcResidualForce().
// TODO test coupled system (auxiliary dynamics depends on q, u depends on
//      auxiliary dynamics).
// TODO trying to set a yDotGuess or Lambda that is of the wrong size.
// smss.getRep().calcConstraintAccelerationErrors
// TODO ensure residuals are NaN if lambdas are not set to 0, unless the system
//      does not have any constraints.
// TODO test calcImplicitResidualsAndConstraintErrors(); make sure constraintErrs
//      is empty if there are no constraints. Make sure lambdaGuess is the
//      correct size.
// TODO test prescribedmotion, model with non-implicit dynamics.
// TODO put multibody residuals calculation in extendComputeImplicitResiduals().
// TODO test what happens when you provide an implicit from but don't set hasImplicitForm.
// TODO test what happens when you set hasImplicitForm() but don't provide it.
// TODO test inheritance hierarchy: multiple subclasses add state vars.
// TODO test passing empty lambdas treats them as 0, same for ydot, with
//      and without constraints.

// TODO mock up the situation where we are minimizing joint contact load
//      while obeying dynamics (in direct collocation fashion--the entire
//      trajectory).

// TODO make sure residual using forward dynamics yDot and lambda gives 0 residual.

// TODO yDotGuess vs ydotGuess

// Implementation:
// TODO Only create implicit cache/discrete vars if any components have an
// implicit form (place in extendAddToSystemAfterComponents()).
// TODO if we use the operator form (not storing residual etc in the state),
// then we have to be clear to implementors of the residual equation that they
// CANNOT depend on qdot, udot, zdot or ydot vectors in the state; that will
// invoke forward dynamics!
// TODO handle quaternion constraints.

//  SparseMatrix jacobian;
//  model.calcImplicitResidualsJacobian(state, yDotGuess, lambdaGuess,
//                                      jacobian);
// 
//  // IN MODEL
// calcPartsOfJacobian(s, dMultibody_dudot, dKinematics_du, dKinematics_dqdot)
//  for (comp : all components) {
//      n, I, J, compJac = comp.computeJacobian(state, yDotGuess);
// 
//      I = [1];
//      J = [10];
// 
//      J = comp.getYIndexForStateVariable("fiber_length");
//      for (k = 1:n) {
//          jacobian[I[k], J[k]] = compJac[k]
//      }
//  }

// TODO sketch out solver-like interface (using IPOPT).


#include <OpenSim/Common/LinearFunction.h>
#include <OpenSim/Simulation/osimSimulation.h>

using namespace OpenSim;
using namespace SimTK;


/** This component provides the differential equation ydot*y = c. In its
implicit form, it has no singularities. In its explicit form ydot = c/y, it 
has a singularity at y = 0. The solutions are:

        y(t) = +/-sqrt(y0^2 + 2*c*t).
                                                                              
It is important that the explicit and implicit forms for this component are
different so that we can ensure the implicit form is used when provided
(in lieu of the trivial implicit form ydot - ydotguess). */
class DynamicsExplicitImplicit : public Component {
OpenSim_DECLARE_CONCRETE_OBJECT(DynamicsExplicitImplicit, Component);
public:
    OpenSim_DECLARE_PROPERTY(default_activ, double,
        "Default value of state variable.");
    OpenSim_DECLARE_PROPERTY(coeff, double,
        "Coefficient in the differential equation.");
    DynamicsExplicitImplicit() {
        constructProperty_default_activ(1.0);
        constructProperty_coeff(-2.0);
    }
protected:
    void extendAddToSystem(SimTK::MultibodySystem& system) const override {
        Super::extendAddToSystem(system);
        addStateVariable("activ", SimTK::Stage::Dynamics, true);
    }
    void computeStateVariableDerivatives(const SimTK::State& s) const override {
        const Real& activ = getStateVariableValue(s, "activ");
        setStateVariableDerivativeValue(s, "activ", get_coeff() / activ);
    }
    // TODO rename to implementComputeImplicitResiduals?
    // TODO provide lambdaGuess?
    void computeImplicitResiduals(const SimTK::State& s,
                                        const SimTK::Vector& yDotGuess,
                                        SimTK::Vector& componentResiduals
                                        ) const override {
        const Real& activ = getStateVariableValue(s, "activ");
        double activDotGuess = getStateVariableDerivativeGuess("activ", yDotGuess);
        // TODO yDotGuess[_activIndex]
        const Real activResidual = activ * activDotGuess - get_coeff();
        setImplicitResidual("activ", activResidual, componentResiduals);
        // TODO residuals[0] = ...;
    }
    void extendInitStateFromProperties(SimTK::State& s) const override {
        Super::extendInitStateFromProperties(s);
        setStateVariableValue(s, "activ", get_default_activ());
    }
    void extendSetPropertiesFromState(const SimTK::State& s) override {
        Super::extendSetPropertiesFromState(s);
        set_default_activ(getStateVariableValue(s, "activ"));
    }
};

/** Differential equation: ydot = y^2. Only explicit form is provided.
We use this to test that the Component interface is able to produce a 
trivial implicit form when the component does not provide an implicit form. */
class QuadraticDynamicsExplicitOnly : public Component {
OpenSim_DECLARE_CONCRETE_OBJECT(QuadraticDynamicsExplicitOnly, Component);
public:
    OpenSim_DECLARE_PROPERTY(default_length, double,
        "Default value of state variable.");
    OpenSim_DECLARE_PROPERTY(coeff, double,
        "Coefficient in the differential equation.");
    QuadraticDynamicsExplicitOnly() {
        constructProperty_default_length(0.0);
        constructProperty_coeff(-1.0);
    }
protected:
    void extendAddToSystem(SimTK::MultibodySystem& system) const override {
        Super::extendAddToSystem(system);
        addStateVariable("length", SimTK::Stage::Dynamics, false);
    }
    void computeStateVariableDerivatives(const SimTK::State& s) const override {
        const Real& activ = getStateVariableValue(s, "length");
        setStateVariableDerivativeValue(s, "length", get_coeff() * activ * activ);
    }
    void extendInitStateFromProperties(SimTK::State& s) const override {
        Super::extendInitStateFromProperties(s);
        setStateVariableValue(s, "length", get_default_length());
    }
    void extendSetPropertiesFromState(const SimTK::State& s) override {
        Super::extendSetPropertiesFromState(s);
        set_default_length(getStateVariableValue(s, "length"));
    }
};

/// Integrates the given system and returns the final state via the `state`
/// argument.
void simulate(const Model& model, State& state, Real finalTime) {
    SimTK::RungeKuttaMersonIntegrator integrator(model.getSystem());
    SimTK::TimeStepper ts(model.getSystem(), integrator);
    ts.initialize(state);
    ts.stepTo(finalTime);
    state = ts.getState();
}

// Ensure that explicit forward integration works for a component that also
// provides an implicit form. The model contains only one component, which
// contains one state variable.
void testExplicitFormOfImplicitComponent() {
    // Create model.
    Model model; model.setName("model");
    auto* comp = new DynamicsExplicitImplicit();
    comp->setName("foo");
    const Real initialValue = 3.5;
    comp->set_default_activ(initialValue);
    const Real coeff = 0.28;
    comp->set_coeff(coeff);
    model.addComponent(comp);
    
    auto s = model.initSystem();
    
    // Check boolean indicators.
    // TODO rename to "hasImplicitForm()"
    SimTK_TEST(comp->hasImplicitFormLocal());
    SimTK_TEST(comp->hasImplicitForm());
    SimTK_TEST(model.hasImplicitFormLocal());
    SimTK_TEST(model.hasImplicitForm());
    
    // Check initial values.
    SimTK_TEST(comp->getStateVariableValue(s, "activ") == initialValue);
    model.realizeAcceleration(s);
    SimTK_TEST_EQ(comp->getStateVariableDerivativeValue(s, "activ"),
                  coeff / initialValue);
    
    // Simulate and check resulting value of state variable.
    const double finalTime = 0.23;
    simulate(model, s, finalTime);
    // Solution is sqrt(y0^2 + 2*c*t).
    SimTK_TEST_EQ_TOL(comp->getStateVariableValue(s, "activ"),
            sqrt(pow(initialValue, 2) + 2 * coeff * finalTime), 1e-5);
}

void testEmptyResidualsIfNoDynamics() {
    Model model;
    State s = model.initSystem();
    Vector residuals(5, 0.0);
    model.calcImplicitResiduals(s, Vector(), Vector(), residuals);
    SimTK_TEST(residuals.size() == 0);
}

void testSingleCustomImplicitEquation() {
    const Real initialValue = 3.5;
    const Real coeff = -0.28;
    const Real activDotGuess = 5.3;
    const Real expectedResidual = initialValue * activDotGuess - coeff;
    
    Model model; model.setName("model");
    auto* comp = new DynamicsExplicitImplicit();
    comp->setName("foo");
    comp->set_default_activ(initialValue);
    comp->set_coeff(coeff);
    model.addComponent(comp);
    
    State s = model.initSystem();
    
    // Set guess.
    Vector yDotGuess(s.getNY());
    comp->setStateVariableDerivativeGuess("activ", activDotGuess, yDotGuess);
    
    // TODO resizing yDotGuess appropriately?
    SimTK_TEST_EQ(yDotGuess, Vector(1, activDotGuess));
    
    // Compute the entire residual vector.
    {
        Vector residuals;
        // TODO rename calcAllImplicitResiduals().
        model.calcImplicitResiduals(s, yDotGuess, Vector(), residuals);
        SimTK_TEST_EQ(residuals, Vector(1, expectedResidual));
        
        // Access specific residual by name from the component:
        SimTK_TEST(comp->getImplicitResidual("activ", residuals) ==
                   expectedResidual);
    }
    
    // Compute the residuals for just the one component.
    {
        Vector componentResiduals =
                comp->calcImplicitResidualsLocal(s, yDotGuess, Vector());
        SimTK_TEST_EQ(componentResiduals, Vector(1, expectedResidual));
    }
}

// Test implicit multibody dynamics (i.e., inverse dynamics) with a point
// mass that can move along the direction of gravity.
void testImplicitMultibodyDynamics1DOF() {
    const double g = 9.81;
    const double mass = 7.2;
    const double u_i  = 3.9;
    
    // Build model.
    // ------------
    Model model; model.setName("model");
    model.setGravity(Vec3(-g, 0, 0)); // gravity pulls in the -x direction.
    auto* body = new OpenSim::Body("ptmass", mass, Vec3(0), Inertia(0));
    auto* slider = new SliderJoint(); slider->setName("slider");
    model.addBody(body);
    model.addJoint(slider);
    slider->updConnector("parent_frame").connect(model.getGround());
    slider->updConnector("child_frame").connect(*body);
    
    State s = model.initSystem();
    
    const auto& coord = slider->getCoordinateSet()[0];
    
    SimTK_TEST(coord.hasImplicitFormLocal());
    SimTK_TEST(model.hasImplicitForm());
    
    coord.setSpeedValue(s, u_i);
    
    const double qDotGuess = 5.6; const double uDotGuess = 8.3;
    
    // Check implicit form.
    // --------------------
    // Set derivative guess.
    Vector yDotGuess(s.getNY());
    // TODO be consistent with "addInControls()" etc.
    coord.setStateVariableDerivativeGuess("value", qDotGuess, yDotGuess);
    coord.setStateVariableDerivativeGuess("speed", uDotGuess, yDotGuess);
   
    // Calculate residual.
    Vector expectedResiduals(2);
    expectedResiduals[0] = u_i - qDotGuess;
    expectedResiduals[1] = mass * uDotGuess - mass * (-g); // M u_dot-f_applied
    Vector residuals;
    model.realizeDynamics(s); // TODO test this requirement.
    model.calcImplicitResiduals(s, yDotGuess, Vector(), residuals);
    // Check the entire residuals vector.
    SimTK_TEST_EQ(residuals, expectedResiduals);
    // Check individual elements of the residual.
    SimTK_TEST_EQ(coord.getImplicitResidual("value", residuals),
                  expectedResiduals[0]);
    SimTK_TEST_EQ(coord.getImplicitResidual("speed", residuals),
                  expectedResiduals[1]);
    
    // TODO set YDotGuess all at once.
    
    
    // Check explicit form.
    // --------------------
    model.realizeAcceleration(s);
    Vector expectedYDot(2);
    expectedYDot[0] = u_i /* = qdot*/; expectedYDot[1] = -g /* = udot*/;
    SimTK_TEST_EQ(s.getYDot(), expectedYDot);
    
    // Passing empty yDotGuess treats them as zeros.
    // ---------------------------------------------
    {
        Vector resEmptyYDot;
        model.calcImplicitResiduals(s, Vector(), Vector(), resEmptyYDot);
        Vector resZerosYDot;
        model.calcImplicitResiduals(s, Vector(2, 0.), Vector(), resZerosYDot);
        SimTK_TEST_EQ(resEmptyYDot, resZerosYDot);
    }
    
    // Error-checking.
    // ---------------
    Vector output;
    // Size of yDotGuess must be correct.
    SimTK_TEST_MUST_THROW_EXC( // yDotGuess too small.
        model.calcImplicitResiduals(s, Vector(1, 0.), Vector(), output),
        OpenSim::Exception);
    SimTK_TEST_MUST_THROW_EXC( // yDotGuess too large.
        model.calcImplicitResiduals(s, Vector(3, 0.), Vector(), output),
        OpenSim::Exception);
    // Size of lambdaGuess must be correct.
    SimTK_TEST_MUST_THROW_EXC( // lambdaGuess too large.
        model.calcImplicitResiduals(s, Vector(2, 0.), Vector(1, 0.), output),
        OpenSim::Exception);
}

// TODO test model with constraints here!

// When components provide dynamics in explicit form only, we compute the
// implicit form by using the explicit form.
void testImplicitFormOfExplicitOnlyComponent() {
    auto createModel = [](double initialValue, double coeff) -> Model {
        Model model; model.setName("model");
        auto* comp = new QuadraticDynamicsExplicitOnly();
        comp->setName("foo");
        comp->set_default_length(initialValue);
        comp->set_coeff(coeff);
        model.addComponent(comp);
        return model;
    };
    
    const Real initialValue = 1.8;
    const Real coeff = -0.73;
    const Real lengthDotGuess = -4.5;
    // The default implicit residual.
    const Real expectedResidual = coeff * pow(initialValue, 2) - lengthDotGuess;
    
    { // Setting elements of guess by name.
        // TODO
        Model model = createModel(initialValue, coeff);
        State s = model.initSystem();
        const auto& comp = model.getComponent("foo");
        
        // Check boolean indicators.
        SimTK_TEST(!comp.hasImplicitFormLocal());
        SimTK_TEST(!comp.hasImplicitForm());
        SimTK_TEST(model.hasImplicitFormLocal());
        SimTK_TEST(!model.hasImplicitForm());
        
        // Set guess.
        Vector yDotGuess(s.getNY());
        comp.setStateVariableDerivativeGuess("length", lengthDotGuess, yDotGuess);
        
        // Calculate residual.
        model.realizeDynamics(s);
        Vector residuals;
        model.calcImplicitResiduals(s, yDotGuess, Vector(), residuals);
        
        // Check the entire residuals vector.
        SimTK_TEST_EQ(residuals, Vector(1, expectedResidual));
        
        // Check individual elements of the residual by name.
        SimTK_TEST(comp.getImplicitResidual("length", residuals) ==
                   expectedResidual);
    }
}

/** If only some of a component's state variables provide an explicit form,
the user should get an error .*/
void testMustHaveFullImplicitForm() {

    // Explicit-only and implicit state var in a single component.
    // -----------------------------------------------------------
    class NotFullImplicitForm : public Component {
    OpenSim_DECLARE_CONCRETE_OBJECT(NotFullImplicitForm, Component);
        void extendAddToSystem(SimTK::MultibodySystem& system) const override {
            Super::extendAddToSystem(system);
            addStateVariable("var_explicit_only");
            addStateVariable("var_explicit_implicit", SimTK::Stage::Dynamics,
                             true);
        }
    };
    
    {
        Model model;
        model.addComponent(new NotFullImplicitForm());
        SimTK_TEST_MUST_THROW_EXC(model.initSystem(), OpenSim::Exception);
    }
    
    
    // Base component does not have implicit form.
    // -------------------------------------------
    class ExplicitOnlyBase : public Component {
    OpenSim_DECLARE_CONCRETE_OBJECT(ExplicitOnlyBase, Component);
    protected:
        void extendAddToSystem(SimTK::MultibodySystem& system) const override {
            Super::extendAddToSystem(system);
            addStateVariable("var_explicit_only");
        }
    };
    class FullImplicitDerived : public ExplicitOnlyBase {
    OpenSim_DECLARE_CONCRETE_OBJECT(FullImplicitDerived, ExplicitOnlyBase);
        void extendAddToSystem(SimTK::MultibodySystem& system) const override {
            Super::extendAddToSystem(system);
            addStateVariable("var_explicit_implicit", SimTK::Stage::Dynamics,
                             true);
        }
    };
    
    {
        Model model;
        model.addComponent(new FullImplicitDerived());
        SimTK_TEST_MUST_THROW_EXC(model.initSystem(), OpenSim::Exception);
    }
}



// =============================================================================
// ImplicitSystemDerivativeSolver
// =============================================================================
// TODO clarify: just solving system of nonlinear equations; no cost function.
// Given a state y and constraints f(y, ydot) = 0, solve for ydot.
class ImplicitSystemDerivativeSolver {
public:
    class Problem; // Defined below.
    ImplicitSystemDerivativeSolver(const Model& model);
    // Solve for the derivatives of the system, ydot, and the Lagrange
    // multipliers, lambda.
    void solve(const State& s, Vector& yDot, Vector& lambda);
private:
    Model m_model;
    std::unique_ptr<Problem> m_problem;
    std::unique_ptr<Optimizer> m_opt;
};
class ImplicitSystemDerivativeSolver::Problem : public SimTK::OptimizerSystem {
public:
    Problem(const ImplicitSystemDerivativeSolver& parent): m_parent(parent) {}
    void setWorkingState(const State& s) { m_workingState = s; }
    int objectiveFunc(const Vector& guess, bool newParams,
                      Real& f) const override {
        f = 0; return 0;
    }
    int constraintFunc(const Vector& guess, bool newParams,
                       Vector& constraints) const override {
        const int ny = m_workingState.getNY();
        const int nu = m_workingState.getNU();
        
        Vector yDotGuess; yDotGuess.viewAssign(guess(0, ny));
        Vector lambdaGuess; lambdaGuess.viewAssign(guess(ny, guess.size() - ny));
        
        Vector residuals; residuals.viewAssign(constraints(0, ny));
        Vector pvaerrs; pvaerrs.viewAssign(constraints(ny, guess.size() - ny));

        // Differential equations.
        m_parent.m_model.realizeDynamics(m_workingState);
        m_parent.m_model.calcImplicitResiduals(m_workingState,
                                                yDotGuess, lambdaGuess,
                                                residuals);

        // Constraints.
        const auto& matter = m_parent.m_model.getMatterSubsystem();
        const auto& uDotGuess = yDotGuess(m_workingState.getUStart(), nu);
        matter.calcConstraintAccelerationErrors(m_workingState, uDotGuess,
                                                pvaerrs);
                                                
        // TODO lambdas will be NaN? residuals will be bad
        // what to do with lambdas for a system without constraints?
        return 0;
    }
private:
    const ImplicitSystemDerivativeSolver& m_parent;
    State m_workingState;
};
ImplicitSystemDerivativeSolver::ImplicitSystemDerivativeSolver(
        const Model& model) : m_model(model), m_problem(new Problem(*this)) {
    // Set up Problem.
    State state = m_model.initSystem();
    const int N = state.getNY() + state.getNMultipliers();
    m_problem->setNumParameters(N);
    m_problem->setNumEqualityConstraints(N);
    Vector limits(N, 50.0); // arbitrary.
    m_problem->setParameterLimits(-limits, limits);
    
    // Set up Optimizer.
    m_opt.reset(new Optimizer(*m_problem, SimTK::InteriorPoint));
    m_opt->useNumericalGradient(true); m_opt->useNumericalJacobian(true);
}
void ImplicitSystemDerivativeSolver::solve(const State& s,
                                           Vector& yDot, Vector& lambda) {
    m_problem->setWorkingState(s);
    Vector results(m_problem->getNumParameters(), 0.0);
    m_opt->optimize(results);
    yDot = results(0, s.getNY());
    lambda = results(s.getNY(), results.size() - s.getNY());
}
// end ImplicitSystemDerivativeSolver...........................................

// TODO explain purpose of this test.
void testGenericInterfaceForImplicitSolver() /*TODO rename test */ {
    Model model;
    auto* comp = new DynamicsExplicitImplicit();
    comp->setName("foo");
    const Real initialValue = 3.5;
    comp->set_default_activ(initialValue);
    const Real coeff = -0.21;
    comp->set_coeff(coeff);
    model.addComponent(comp);
    State s = model.initSystem();
    
    // Computing yDot using implicit form.
    Vector yDotImplicit;
    {
        State sImplicit = s;
        ImplicitSystemDerivativeSolver solver(model);
        Vector lambda;
        solver.solve(sImplicit, yDotImplicit, lambda);
        // There should be no multipliers.
        SimTK_TEST(lambda.size() == 0);
    }
    
    // Computing yDot using explicit form.
    Vector yDotExplicit;
    {
        State sExplicit = s;
        model.realizeAcceleration(sExplicit);
        yDotExplicit = sExplicit.getYDot();
    }
    
    // Implicit and explicit forms give same yDot.
    SimTK_TEST_EQ_TOL(yDotImplicit, yDotExplicit, 1e-11);
    // Also make sure the test is actually testing something.
    SimTK_TEST(yDotExplicit.size() == 1);
    SimTK_TEST_NOTEQ(yDotExplicit.norm(), 0);
}

void testImplicitSystemDerivativeSolverMultibody1DOF() {
    Model model; model.setName("model");
    model.setGravity(Vec3(-9.81, 0, 0)); // gravity pulls in the -x direction.
    auto* body = new OpenSim::Body("ptmass", 1.3, Vec3(0), Inertia(0));
    auto* slider = new SliderJoint(); slider->setName("slider");
    model.addBody(body);
    model.addJoint(slider);
    slider->updConnector("parent_frame").connect(model.getGround());
    slider->updConnector("child_frame").connect(*body);
    
    State s = model.initSystem();
    const auto& coord = slider->getCoordinateSet()[0];
    coord.setSpeedValue(s, 1.7);
    
    // Computing yDot using implicit form.
    Vector yDotImplicit;
    {
        State sImplicit = s;
        ImplicitSystemDerivativeSolver solver(model);
        Vector lambda; // unused.
        solver.solve(sImplicit, yDotImplicit, lambda);
    }
    
    // Computing yDot using explicit form.
    Vector yDotExplicit;
    {
        State sExplicit = s;
        model.realizeAcceleration(sExplicit);
        yDotExplicit = sExplicit.getYDot();
    }
    
    // Implicit and explicit forms give same yDot.
    SimTK_TEST_EQ_TOL(yDotImplicit, yDotExplicit, 1e-8);
    
    // Also make sure the test is actually testing something.
    SimTK_TEST(yDotExplicit.size() == 2);
    SimTK_TEST_NOTEQ(yDotExplicit.norm(), 0);
}

void testCoordinateCouplerConstraint() {
    Model model; model.setName("twodof");
    auto* body = new OpenSim::Body("ptmass", 1.5, Vec3(0.7, 0, 0),
                                   Inertia::ellipsoid(1, 2, 3));
    // TODO BallJoint() causes issues.
    auto* joint = new GimbalJoint(); joint->setName("joint");
    auto& c0 = joint->upd_CoordinateSet()[0]; c0.setName("c0");
    auto& c1 = joint->upd_CoordinateSet()[1]; c1.setName("c1");
    auto& c2 = joint->upd_CoordinateSet()[2]; c2.setName("c2");
    model.addBody(body);
    model.addJoint(joint);
    joint->updConnector("parent_frame").connect(model.getGround());
    joint->updConnector("child_frame").connect(*body);
    
    // Set up constraint using a linear relation between c0 and c1.
    auto* coupler = new CoordinateCouplerConstraint(); coupler->setName("cplr");
    model.addConstraint(coupler);
    Array<std::string> indepCoords; indepCoords.append("c0");
    coupler->setIndependentCoordinateNames(indepCoords);
    coupler->setDependentCoordinateName("c1");
    const Real slope = 5.1; const Real intercept = 2.31;
    coupler->setFunction(LinearFunction(slope, intercept));
    
    State s = model.initSystem();
    c0.setValue(s, 0.51, false); // We'll enforce constraints all at once.
    c2.setValue(s, 0.36, false);
    c0.setSpeedValue(s, 1.5); // The projection will change this value.
    c2.setSpeedValue(s, 2.8);
    model.assemble(s);
    model.getSystem().projectU(s); // Enforce velocity constraints.
    
    // Ensure that the constraints are obeyed in the current state.
    SimTK_TEST_EQ(c1.getValue(s), slope * c0.getValue(s) + intercept);
    // Check the velocity-level constraint:
    SimTK_TEST_EQ(c1.getSpeedValue(s), slope * c0.getSpeedValue(s));
    
    // Computing yDot and lambda using implicit form.
    Vector yDotImplicit, lambdaImplicit;
    {
        State sImplicit = s;
        ImplicitSystemDerivativeSolver solver(model);
        solver.solve(sImplicit, yDotImplicit, lambdaImplicit);
        
        // Acceleration-level constraint equation is satisfied.
        SimTK_TEST_EQ_TOL(yDotImplicit[1], slope * yDotImplicit[0], 1e-12);
    }
    
    // Computing yDot and lambda using explicit form.
    Vector yDotExplicit, lambdaExplicit;
    {
        State sExplicit = s;
        model.realizeAcceleration(sExplicit);
        yDotExplicit = sExplicit.getYDot();
        lambdaExplicit = sExplicit.getMultipliers();
        
        // Acceleration-level constraint equation is satisfied.
        SimTK_TEST_EQ_TOL(c1.getAccelerationValue(sExplicit),
                          slope * c0.getAccelerationValue(sExplicit), 1e-9);
    }
    
    
    // Implicit and explicit forms give same yDot and lambda.
    SimTK_TEST_EQ_TOL(yDotImplicit, yDotExplicit, 1e-9);
    SimTK_TEST_EQ_TOL(lambdaImplicit, lambdaExplicit, 1e-9);
    
    // Also make sure the test is actually testing something.
    SimTK_TEST(yDotExplicit.size() == 6); // 3 DOFs, 2 states per DOF.
    SimTK_TEST(lambdaExplicit.size() == 1);
}

void testErrorsForUnsupportedModels() {
    // TODO constraints? does not require error message.
    // TODO prescribed motion.
}

int main() {
    SimTK_START_TEST("testImplicitDifferentialEquations");
        SimTK_SUBTEST(testExplicitFormOfImplicitComponent);
        SimTK_SUBTEST(testEmptyResidualsIfNoDynamics);
        SimTK_SUBTEST(testSingleCustomImplicitEquation);
        SimTK_SUBTEST(testImplicitMultibodyDynamics1DOF);
        SimTK_SUBTEST(testImplicitFormOfExplicitOnlyComponent);
        SimTK_SUBTEST(testMustHaveFullImplicitForm);
        // TODO SimTK_SUBTEST(testImplicitFormOfCombinedImplicitAndExplicitComponents);
        // TODO SimTK_SUBTEST(testMultibody1DOFAndCustomComponent);
        SimTK_SUBTEST(testGenericInterfaceForImplicitSolver);
        SimTK_SUBTEST(testImplicitSystemDerivativeSolverMultibody1DOF);
        SimTK_SUBTEST(testCoordinateCouplerConstraint); // TODO rename
        SimTK_SUBTEST(testErrorsForUnsupportedModels);
    SimTK_END_TEST();
}





