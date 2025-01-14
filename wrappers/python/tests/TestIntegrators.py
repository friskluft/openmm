import unittest
import warnings
import tempfile
from datetime import datetime, timedelta
from openmm import *
from openmm.app import *
from openmm.unit import *
import math, random

class TestIntegrators(unittest.TestCase):
    """Test Python Integrator classes"""

    def testMTSIntegratorExplicit(self):
        """Test the MTS integrator on an explicit solvent system"""
        # Create a periodic solvated system with PME
        pdb = PDBFile('systems/alanine-dipeptide-explicit.pdb')
        ff = ForceField('amber99sbildn.xml', 'tip3p.xml')
        system = ff.createSystem(pdb.topology, nonbondedMethod=PME)

        # Split forces into groups
        for force in system.getForces():
            if force.__class__.__name__ == 'NonbondedForce':
                force.setForceGroup(1)
                force.setReciprocalSpaceForceGroup(2)
            else:
                force.setForceGroup(0)

        # Create an integrator
        integrator = MTSIntegrator(4*femtoseconds, [(2,1), (1,2), (0,8)])

        # Run a few steps of dynamics
        context = Context(system, integrator)
        context.setPositions(pdb.positions)
        integrator.step(10)

        # Ensure energy is well-behaved.
        state = context.getState(getEnergy=True)
        if not (state.getPotentialEnergy() / kilojoules_per_mole < 0.0):
            raise Exception('Potential energy of alanine dipeptide system with MTS integrator is blowing up: %s' % str(state.getPotentialEnergy()))

    def testMTSIntegratorConstraints(self):
        """Test the MTS integrator energy conservation on a system of constrained particles with no inner force (just constraints)"""

        # Create a constrained test system
        numParticles = 8
        numConstraints = 5
        system = System()
        force = NonbondedForce()
        for i in range(numParticles):
            system.addParticle(5.0 if i%2==0 else 10.0)
            force.addParticle((0.2 if i%2==0 else -0.2), 0.5, 5.0);
        system.addConstraint(0, 1, 1.0);
        system.addConstraint(1, 2, 1.0);
        system.addConstraint(2, 3, 1.0);
        system.addConstraint(4, 5, 1.0);
        system.addConstraint(6, 7, 1.0);
        system.addForce(force)

        # Create integrator where inner timestep just evaluates constraints
        integrator = MTSIntegrator(1*femtoseconds, [(1,1), (0,4)])
        integrator.setConstraintTolerance(1e-5);

        positions = [ (i/2., (i+1)/2., 0.) for i in range(numParticles) ]
        velocities = [ (random.random()-0.5, random.random()-0.5, random.random()-0.5) for i in range(numParticles) ]

        # Create Context
        platform = Platform.getPlatformByName('Reference')
        context = Context(system, integrator, platform)
        context.setPositions(positions)
        context.setVelocities(velocities)
        context.applyConstraints(1e-5)

        # Simulate it and see whether the constraints remain satisfied.
        CONSTRAINT_RELATIVE_TOLERANCE = 1.e-4 # relative constraint violation tolerance
        ENERGY_RELATIVE_TOLERANCE = 1.e-2 # relative energy violation tolerance
        for i in range(1000):
            state = context.getState(getPositions=True, getEnergy=True)
            positions = state.getPositions()
            for j in range(numConstraints):
                [particle1, particle2, constraint_distance] = system.getConstraintParameters(j)
                current_distance = 0.0 * nanometers**2
                for k in range(3):
                    current_distance += (positions[particle1][k] - positions[particle2][k])**2
                current_distance = sqrt(current_distance)
                # Fail test if outside of relative tolerance
                relative_violation = (current_distance - constraint_distance) / constraint_distance
                if (relative_violation > CONSTRAINT_RELATIVE_TOLERANCE):
                    raise Exception('Constrained distance is violated by relative tolerance of %f (constraint %s actual %s)' % (relative_violation, str(constraint_distance), str(current_distance)))
            # Check total energy
            total_energy = state.getPotentialEnergy() + state.getKineticEnergy()
            if (i == 1):
                initial_energy = total_energy
            elif (i > 1):
                relative_violation = abs((total_energy - initial_energy) / initial_energy)
                if (relative_violation > ENERGY_RELATIVE_TOLERANCE):
                    raise Exception('Total energy is violated by relative tolerance of %f on step %d (initial %s final %s)' % (relative_violation, i, str(initial_energy), str(total_energy)))
            # Take a step
            integrator.step(1)

    def testBadGroups(self):
        """Test the MTS integrator with bad force group substeps."""
        # Create a periodic solvated system with PME
        pdb = PDBFile('systems/alanine-dipeptide-explicit.pdb')
        ff = ForceField('amber99sbildn.xml', 'tip3p.xml')
        system = ff.createSystem(pdb.topology, nonbondedMethod=PME)

        # Split forces into groups
        for force in system.getForces():
            if force.__class__.__name__ == 'NonbondedForce':
                force.setForceGroup(1)
                force.setReciprocalSpaceForceGroup(2)
            else:
                force.setForceGroup(0)

        with self.assertRaises(ValueError):
            # Create an integrator
            integrator = MTSIntegrator(4*femtoseconds, [(2,1), (1,3), (0,8)])

            # Run a few steps of dynamics
            context = Context(system, integrator)
            context.setPositions(pdb.positions)
            integrator.step(10)

    def testMTSLangevinIntegrator(self):
        """Test the MTSLangevinIntegrator on an explicit solvent system"""
        # Create a periodic solvated system with PME
        pdb = PDBFile('systems/alanine-dipeptide-explicit.pdb')
        ff = ForceField('amber99sbildn.xml', 'tip3p.xml')
        system = ff.createSystem(pdb.topology, nonbondedMethod=PME)

        # Split forces into groups
        for force in system.getForces():
            if force.__class__.__name__ == 'NonbondedForce':
                force.setForceGroup(1)
                force.setReciprocalSpaceForceGroup(2)
            else:
                force.setForceGroup(0)

        # Create an integrator
        integrator = MTSLangevinIntegrator(300*kelvin, 5/picosecond, 4*femtoseconds, [(2,1), (1,2), (0,4)])

        # Run some equilibration.
        context = Context(system, integrator)
        context.setPositions(pdb.positions)
        context.setVelocitiesToTemperature(300*kelvin)
        integrator.step(500)

        # See if the temperature is correct.
        totalEnergy = 0*kilojoules_per_mole
        steps = 50
        for i in range(steps):
            integrator.step(10)
            totalEnergy += context.getState(getEnergy=True).getKineticEnergy()
        averageEnergy = totalEnergy/steps
        dofs = 3*system.getNumParticles() - system.getNumConstraints() - 3
        temperature = averageEnergy*2/(dofs*MOLAR_GAS_CONSTANT_R)
        self.assertTrue(290*kelvin < temperature < 310*kelvin)

    def testMTSLangevinIntegratorFriction(self):
        """Test the MTSLangevinIntegrator on a force-free particle to ensure friction is properly accounted for (issue #3790)"""
        # Create a System with a single particle and no forces
        system = System()
        system.addParticle(12.0*amu)
        platform = Platform.getPlatformByName('Reference')
        initial_positions = [Vec3(0,0,0)]
        initial_velocities = [Vec3(1,0,0)]
        nsteps = 125 # number of steps to take
        collision_rate = 1/picosecond
        timestep = 4*femtoseconds

        def get_final_velocities(nsubsteps):
            """Get the final velocity vector after a fixed number of steps for the specified number of substeps"""
            integrator = MTSLangevinIntegrator(0*kelvin, collision_rate, timestep, [(0,nsubsteps)])
            context = Context(system, integrator, platform)
            context.setPositions(initial_positions)
            context.setVelocities(initial_velocities)
            integrator.step(nsteps)
            final_velocities = context.getState(getVelocities=True).getVelocities()
            del context, integrator
            return final_velocities

        # Compare sub-stepped MTS with single-step MTS
        for nsubsteps in range(2,6):
            mts_velocities = get_final_velocities(nsubsteps)
            self.assertAlmostEqual(math.exp(-timestep*nsteps*collision_rate), mts_velocities[0].x)
            self.assertAlmostEqual(0, mts_velocities[0].y)
            self.assertAlmostEqual(0, mts_velocities[0].z)

    def testNoseHooverIntegrator(self):
        """Test partial thermostating in the NoseHooverIntegrator (only API)"""
        pdb = PDBFile('systems/alanine-dipeptide-explicit.pdb')
        ff = ForceField('amber99sbildn.xml', 'tip3p.xml')
        system = ff.createSystem(pdb.topology, nonbondedMethod=PME)

        integrator = NoseHooverIntegrator(1.0*femtosecond)
        integrator.addSubsystemThermostat(list(range(5)), [], 200*kelvin, 1/picosecond, 200*kelvin, 1/picosecond, 3,3,3)
        con = Context(system, integrator)
        con.setPositions(pdb.positions)

        integrator.step(5)
        self.assertNotEqual(integrator.computeHeatBathEnergy(), 0.0*kilojoule_per_mole)

    def testDrudeNoseHooverIntegrator(self):
        """Test the DrudeNoseHooverIntegrator"""
        warnings.filterwarnings('ignore', category=CharmmPSFWarning)
        psf = CharmmPsfFile('systems/ala3_solv_drude.psf')
        crd = CharmmCrdFile('systems/ala3_solv_drude.crd')
        params = CharmmParameterSet('systems/toppar_drude_master_protein_2013e.str')
        # Box dimensions (cubic box)
        psf.setBox(33.2*angstroms, 33.2*angstroms, 33.2*angstroms)

        system = psf.createSystem(params, nonbondedMethod=PME, ewaldErrorTolerance=0.0005)
        integrator = DrudeNoseHooverIntegrator(300*kelvin, 1.0/picosecond, 1*kelvin, 10/picosecond, 0.001*picoseconds)
        con = Context(system, integrator)
        con.setPositions(crd.positions)

        integrator.step(5)
        self.assertNotEqual(integrator.computeHeatBathEnergy(), 0.0*kilojoule_per_mole)

if __name__ == '__main__':
    unittest.main()
