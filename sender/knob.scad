$fs=0.2;
$fa=1;

knob_dia=71;
knob_height=19;
knob_cut_angle=46.5;
knob_knurl_depth=1;
knob_dividers=12;  // Should be the number of detents in rotatry encoder
knob_top_roundness=5;

module base_knob(h=knob_height) {
    hull() {
	cylinder(r=knob_dia/2,h=1);
	translate([0,0,h - knob_top_roundness])
	   rotate_extrude(convexity=10) translate([knob_dia/2 - knob_top_roundness,0,0]) circle(r=knob_top_roundness);
    }
}

function polar_x(r, angle) = r * cos(angle);
function polar_y(r, angle) = r * sin(angle);
function knurl(fraction, knurl_dia=knob_knurl_depth/2) = sin(fraction * 360) * knurl_dia - knurl_dia;

module knob_slice(r=knob_dia/2,segments=knob_dividers) {
    a = 360 / knob_dividers / 10;
    poly = [[0,0],
	[polar_x(r + knurl(0),   0*0), polar_y(r + knurl(0),0*a)],
	[polar_x(r + knurl(1/10),1*a), polar_y(r + knurl(1/10),1*a)],
	[polar_x(r + knurl(2/10),2*a), polar_y(r + knurl(2/10),2*a)],
	[polar_x(r + knurl(3/10),3*a), polar_y(r + knurl(3/10),3*a)],
	[polar_x(r + knurl(4/10),4*a), polar_y(r + knurl(4/10),4*a)],
	[polar_x(r + knurl(5/10),5*a), polar_y(r + knurl(5/10),5*a)],
	[polar_x(r + knurl(6/10),6*a), polar_y(r + knurl(6/10),6*a)],
	[polar_x(r + knurl(7/10),7*a), polar_y(r + knurl(7/10),7*a)],
	[polar_x(r + knurl(8/10),8*a), polar_y(r + knurl(8/10),8*a)],
	[polar_x(r + knurl(9/10),9*a), polar_y(r + knurl(9/10),9*a)],
	[polar_x(r + knurl(10/10),10*a), polar_y(r + knurl(10/10),10*a)],
	// Add a little bit of overlap to the next slice:
	[polar_x(r + knurl(11/10),11*a), polar_y(r + knurl(11/10),11*a)],
        [0,0]];
    polygon(points=poly, convexity=2);
}

module knurls(h=knob_height, angle_offset=90 / knob_dividers) {
    linear_extrude(height=h) for (i=[0:360/knob_dividers:359]) {
	rotate([0,0,i+angle_offset]) knob_slice();
    }
}

module knurl_knob(h=knob_height) {
    intersection() {
	knurls(h);
	base_knob(h);
    }
}
