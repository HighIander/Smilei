# ----------------------------------------------------------------------------------------
# 					SIMULATION PARAMETERS FOR THE PIC-CODE SMILEI
# ----------------------------------------------------------------------------------------

import math
l0 = 2.0*math.pi	# wavelength in normalized units
t0 = l0				# optical cycle in normalized units
rest = 6000.0		# nb of timestep in 1 optical cycle
resx = 4000.0		# nb cells in 1 wavelength
Lsim = 0.01*l0	    # simulation length
Tsim = 0.2*t0		# duration of the simulation


Main(
    geometry = "1Dcartesian",
     
    interpolation_order = 2,
     
    cell_length = [l0/resx],
    sim_length  = [Lsim],
    
    number_of_patches = [ 4 ],
    
    timestep = t0/rest,
    sim_time = Tsim,
     
    EM_boundary_conditions = [ ['silver-muller'] ],
    
    reference_angular_frequency_SI = 6*math.pi*1e14,
    
    random_seed = smilei_mpi_rank
)

Species(
	name = 'hydrogen',
	ionization_model = 'tunnel',
	ionization_electrons = 'electron',
	atomic_number = 1,
	position_initialization = 'regular',
	momentum_initialization = 'cold',
	particles_per_cell = 10,
	mass = 1836.0*1000.,
	charge = 0.0,
	nb_density = 0.1,
	boundary_conditions = [
		["periodic", "periodic"],
	],
)

Species(
	name = 'carbon',
	ionization_model = 'tunnel',
	ionization_electrons = 'electron',
	atomic_number = 6,
	position_initialization = 'regular',
	momentum_initialization = 'cold',
	particles_per_cell = 10,
	mass = 1836.0*1000.,
	charge = 0.0,
	nb_density = 0.1,
	boundary_conditions = [
		["periodic", "periodic"],
	],
)

Species(
	name = 'electron',
	position_initialization = 'regular',
	momentum_initialization = 'cold',
	particles_per_cell = 0,
	mass = 1.0,
	charge = -1.0,
	charge_density = 0.0,
	boundary_conditions = [
		["periodic", "periodic"],
	],
)

LaserPlanar1D(
	box_side = 'xmin',
	a0 = 0.1,
    omega = 1.,
    polarization_phi = 0.,
    time_envelope = tconstant(),
)

DiagScalar(every = 20)

DiagFields(
    every = 20,
    time_average = 1,
    fields = ["Ex", "Ey", "Ez"]
)

DiagParticleBinning(
	output = "density",
	every = 20,
	species = ["hydrogen"],
	axes = [
		["charge",  -0.5, 1.5, 2]
	]
)

DiagParticleBinning(
	output = "density",
	every = 20,
	species = ["carbon"],
	axes = [
		["charge",  -0.5, 6.5, 7]
	]
)

DiagTrackParticles(
	species = "electron",
	every = 30
)