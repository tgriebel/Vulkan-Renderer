import numpy as np
import matplotlib.pyplot as plt
from matplotlib.widgets import Slider


# ---------------------------------------------------------------
# Sample generation — Halton sequence mapped to unit disk via
# Shirley concentric mapping. Gives a well-distributed set for
# any sample count without hardcoding.
# ---------------------------------------------------------------

def halton( index, base ):
    result = 0.0
    f = 1.0
    i = index
    while i > 0:
        f /= base
        result += f * ( i % base )
        i //= base
    return result


def generate_disk_samples( count ):
    samples = []
    for i in range( 1, count + 1 ):
        u = halton( i, 2 )
        v = halton( i, 3 )

        # Shirley concentric mapping [0,1]^2 -> unit disk
        a = 2.0 * u - 1.0
        b = 2.0 * v - 1.0

        if abs( a ) > abs( b ):
            r = abs( a )
            theta = ( np.pi / 4.0 ) * ( b / a ) if a != 0.0 else 0.0
            if a < 0.0:
                theta += np.pi
        elif abs( b ) > 0.0:
            r = abs( b )
            theta = ( np.pi / 2.0 ) - ( np.pi / 4.0 ) * ( a / b )
            if b < 0.0:
                theta += np.pi
        else:
            r, theta = 0.0, 0.0

        samples.append( [ r * np.cos( theta ), r * np.sin( theta ) ] )

    return np.array( samples )


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
INIT_SAMPLE_COUNT = 16
INIT_APERTURE_DEG = 30.0

theta_ref = np.linspace( 0, 2 * np.pi, 300 )

# ---------------------------------------------------------------
# Figure layout — main axes + slider axes below
# ---------------------------------------------------------------

fig = plt.figure( figsize=( 12, 7 ) )
fig.suptitle( "NGonBoundaryRadius — shape and sample distribution", fontsize=13 )

ax_shape = fig.add_axes( [ 0.05, 0.30, 0.42, 0.60 ] )
ax_samples = fig.add_axes( [ 0.53, 0.30, 0.42, 0.60 ] )

ax_sl_n = fig.add_axes( [ 0.15, 0.18, 0.70, 0.03 ] )
ax_sl_count = fig.add_axes( [ 0.15, 0.12, 0.70, 0.03 ] )
ax_sl_angle = fig.add_axes( [ 0.15, 0.06, 0.70, 0.03 ] )

sl_n = Slider( ax_sl_n, "Blades (N)", 3, 12, valinit=INIT_N, valstep=1 )
sl_count = Slider( ax_sl_count, "Sample count", 4, 1024, valinit=INIT_SAMPLE_COUNT, valstep=1 )
sl_angle = Slider( ax_sl_angle, "Aperture angle (°)", 0.0, 360.0, valinit=INIT_APERTURE_DEG )


# ---------------------------------------------------------------
# Plot helpers
# ---------------------------------------------------------------

def draw_shape( ax, N ):
    ax.cla()
    bx, by = ngon_boundary_xy( N )
    ax.plot( bx, by, linewidth=1.8, label=f"N={N} boundary" )
    ax.plot( np.cos( theta_ref ), np.sin( theta_ref ),
             color="grey", linestyle="--", linewidth=0.8, label="unit circle" )
    ax.set_title( f"{N}-gon boundary" )
    ax.set_aspect( "equal" )
    ax.legend( fontsize=8 )
    ax.grid( True, linewidth=0.4 )


def draw_samples( ax, N, count, aperture_angle ):
    ax.cla()
    bx, by = ngon_boundary_xy( N )
    ax.plot( bx, by, linewidth=1.8, label=f"N={N} boundary" )

    samples = generate_disk_samples( count )
    sx, sy = warp_samples_to_ngon( samples, N, aperture_angle )
    ax.scatter( sx, sy, s=18, zorder=5, label=f"{count} samples" )

    ax.set_title( f"Warped samples  (angle={np.degrees(aperture_angle):.0f}°)" )
    ax.set_aspect( "equal" )
    ax.legend( fontsize=8 )
    ax.grid( True, linewidth=0.4 )


def refresh( _ ):
    N = int( sl_n.val )
    count = int( sl_count.val )
    aperture_angle = np.radians( sl_angle.val )
    draw_shape( ax_shape, N )
    draw_samples( ax_samples, N, count, aperture_angle )
    fig.canvas.draw_idle()


sl_n.on_changed( refresh )
sl_count.on_changed( refresh )
sl_angle.on_changed( refresh )

# Initial draw
refresh( None )

plt.show()
