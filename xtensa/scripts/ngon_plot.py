import numpy as np
import matplotlib.pyplot as plt
from matplotlib.widgets import Slider


# ---------------------------------------------------------------
# Sample generation — NxN grid mapped to unit disk via
# Shirley concentric mapping.
# ---------------------------------------------------------------

def shirley_concentric( a, b ):
    r   = np.where( np.abs( a ) > np.abs( b ), a, b )
    phi = np.where(
        np.abs( a ) > np.abs( b ),
        ( np.pi / 4.0 ) * ( b / np.where( a != 0, a, 1 ) ),
        ( np.pi / 2.0 ) - ( np.pi / 4.0 ) * ( a / np.where( b != 0, b, 1 ) )
    )
    phi = np.where( ( a == 0 ) & ( b == 0 ), 0.0, phi )
    r   = np.where( ( a == 0 ) & ( b == 0 ), 0.0, r )
    return r * np.cos( phi ), r * np.sin( phi )


def generate_grid( n ):
    t = np.linspace( 0.0, 1.0, n )
    us, vs = np.meshgrid( t, t )
    a = 2.0 * us.ravel() - 1.0
    b = 2.0 * vs.ravel() - 1.0
    x, y = shirley_concentric( a, b )
    return np.column_stack( ( x, y ) )


# ---------------------------------------------------------------
# NGon
# ---------------------------------------------------------------

def ngon_boundary_radius( theta, N ):
    pi_over_n = np.pi / N
    two_pi_over_n = 2.0 * pi_over_n
    t = theta - two_pi_over_n * np.floor( theta / two_pi_over_n ) - pi_over_n
    return np.cos( pi_over_n ) / np.cos( t )


def ngon_boundary_xy( N ):
    theta = np.linspace( 0, 2 * np.pi, 1000 )
    r = ngon_boundary_radius( theta, N )
    return r * np.cos( theta ), r * np.sin( theta )


def warp_samples_to_ngon( samples, N, aperture_angle ):
    xs, ys = [], []
    for s in samples:
        r = np.linalg.norm( s )
        theta = np.arctan2( s[1], s[0] ) + aperture_angle
        r *= ngon_boundary_radius( theta, N )
        xs.append( r * np.cos( theta ) )
        ys.append( r * np.sin( theta ) )
    return np.array( xs ), np.array( ys )


# ---------------------------------------------------------------
# Initial values
# ---------------------------------------------------------------

INIT_N = 6
INIT_GRID_N = 4
INIT_APERTURE_DEG = 30.0

theta_ref = np.linspace( 0, 2 * np.pi, 300 )

# ---------------------------------------------------------------
# Figure layout
# ---------------------------------------------------------------

fig = plt.figure( figsize=( 13, 8 ) )
fig.suptitle( "NGonBoundaryRadius — shape and sample distribution", fontsize=13 )

ax_shape   = fig.add_axes( [ 0.05, 0.32, 0.42, 0.60 ] )
ax_samples = fig.add_axes( [ 0.53, 0.32, 0.42, 0.60 ] )

ax_sl_grid  = fig.add_axes( [ 0.15, 0.20, 0.70, 0.03 ] )
ax_sl_n     = fig.add_axes( [ 0.15, 0.13, 0.70, 0.03 ] )
ax_sl_angle = fig.add_axes( [ 0.15, 0.06, 0.70, 0.03 ] )

sl_grid  = Slider( ax_sl_grid,  "Grid N",             1,   32,    valinit=INIT_GRID_N,       valstep=1 )
sl_n     = Slider( ax_sl_n,     "Blades (N)",          3,   12,    valinit=INIT_N,            valstep=1 )
sl_angle = Slider( ax_sl_angle, "Aperture angle (°)",  0.0, 360.0, valinit=INIT_APERTURE_DEG )


# ---------------------------------------------------------------
# Plot helpers
# ---------------------------------------------------------------

def draw_shape( ax, N, samples ):
    ax.cla()
    bx, by = ngon_boundary_xy( N )
    ax.plot( bx, by, linewidth=1.8, label=f"N={N} boundary" )
    ax.plot( np.cos( theta_ref ), np.sin( theta_ref ),
             color="grey", linestyle="--", linewidth=0.8, label="unit circle" )
    ax.scatter( samples[:, 0], samples[:, 1], s=18, zorder=5, label=f"{len(samples)} samples" )
    ax.set_title( f"{N}-gon boundary" )
    ax.set_aspect( "equal" )
    ax.legend( fontsize=8 )
    ax.grid( True, linewidth=0.4 )


def draw_samples( ax, N, samples, aperture_angle ):
    ax.cla()
    bx, by = ngon_boundary_xy( N )
    ax.plot( bx, by, linewidth=1.8, label=f"N={N} boundary" )

    sx, sy = warp_samples_to_ngon( samples, N, aperture_angle )
    ax.scatter( sx, sy, s=18, zorder=5, label=f"{len(samples)} samples" )

    ax.set_title( f"Warped samples  (angle={np.degrees(aperture_angle):.0f}°)" )
    ax.set_aspect( "equal" )
    ax.legend( fontsize=8 )
    ax.grid( True, linewidth=0.4 )


def refresh( _ ):
    N = int( sl_n.val )
    aperture_angle = np.radians( sl_angle.val )
    samples = generate_grid( int( sl_grid.val ) )
    draw_shape( ax_shape, N, samples )
    draw_samples( ax_samples, N, samples, aperture_angle )
    fig.canvas.draw_idle()


sl_grid.on_changed( refresh )
sl_n.on_changed( refresh )
sl_angle.on_changed( refresh )

refresh( None )

plt.show()
