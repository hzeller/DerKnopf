include <knob.scad>

$fn=180;
epsilon=0.02;
clearance = 0.35;
pcb_width=14;
pcb_distance=1;
diameter=65;   // TODO: calculate.
batt_len=44.5 + 6 + 2*clearance;  // Additional space for contacts
batt_dia=10.5 + 2*clearance;
batt_blade_thick=1.5;
font="Helvetica:style=Bold";
total_height=28;
vertical_divide=15;
knob_overlap=6;
bottom_thick=3.2;  // acrylic
led_mount_dia=6;
led_front=4.5;
case_knob_clearance=1;

module led(dia=5) {
    // Nice infrared purple :)
    color("purple") {
	sphere(r=dia/2);
	cylinder(r=dia/2,h=8.5 - dia/2);
	translate([0,0,8.5-dia/2-2]) cylinder(r=dia/2 + 0.5, h=2);
    }
}

module battery(length=batt_len, dia=batt_dia,negatives=true) {
    translate([-batt_len/2,0,0]) {
	translate([0,0,dia/2]) rotate([0,90,0]) cylinder(r=dia/2, h=length);
	translate([0,-dia/2,0]) cube([length, dia, dia/2]);
	if (negatives) {
	    // Contact blades
	    translate([0,-0.6*dia,-1]) cube([batt_blade_thick, 30, dia+1]);
	    translate([batt_len-1,-0.6*dia,-1]) cube([batt_blade_thick, 1.4*dia,dia+1]);

	    // +/- text
	    translate([0,0,dia/2]) rotate([0,0,0]) union(){
		translate([10,-6/2,0]) color("black") linear_extrude(height=dia/2 + 0.6) text("–", size=8, font=font);
		translate([length-15,-6/2,0]) color("red") linear_extrude(height=dia/2 + 0.6) text("+", size=8, font=font);
	    }
	}
    }
}

module m3_bolt(h=2.6) {
    cylinder(r=5.7/2, h=h, $fn=6);
}

module remote_control(l=37, w=pcb_width, h=9,negatives=true) {
    translate([-l/2,-w/2,0]) cube([l,w,h]);
    if (negatives) color("lightblue") {
	difference() {    // turn shaft
	    cylinder(r=6.4/2, h=h + 14.55 + 0.5/*fudge*/);
	    // ... with flattened side.
	    translate([-4, 1.6, -epsilon]) cube([8, 8, 25+2*epsilon]);
	}
	cylinder(r=9/2 + clearance, h=h + 6.8);  // mount shaft

	// set screw
	translate([0,0,h + 10]) rotate([-90, 0, 0]) union() {
	    cylinder(r=3.2/2, h=50);
	    translate([0,0,3]) {
		// Space for bolt
		rotate([0,0,30]) m3_bolt();
		translate([-5.5/2,0,0]) cube([5.5, 6, 2.6]);
	    }
	    translate([0,0,9.5]) {
		// Space to drop screw in.
		cylinder(r=6/2, h=12);
		translate([0,0,12-4]) translate([-6/2,0,0]) cube([6, 7, 4]);
		translate([-3/2,0,0]) cube([3, 7, 12]);
	    }
	}

	// Mounting gear on encoder
	translate([0,0,h + 2.9]) cylinder(r=16/2 + clearance, h=3.5);
	// Retainer clip for encoder
	translate([6, -3/2, 0]) cube([2, 3, h+3]);
    }

    if (negatives) {
	// Infrared diodes
	// Mounting holes
	translate([0,0,led_mount_dia/2-2]) {
	    rotate([0,90,0]) translate([0,0,-33]) cylinder(r=led_mount_dia/2, h=66);
	    // LEDs
	    translate([-knob_dia/2+2,0,0]) rotate([0,90,0]) led();
	    translate([knob_dia/2-2,0,0]) rotate([0,-90,0]) led();
	}
    }
}

module two_batt_module(negatives=true) {
    translate([-batt_len/2,0,0]) {
	translate([batt_len/2,-(batt_dia+pcb_width)/2-pcb_distance,0]) battery(negatives=negatives);
	translate([batt_len/2,+(batt_dia+pcb_width)/2+pcb_distance,0]) rotate([0,0,180]) battery(negatives=negatives);
    }
}

module three_batt_module() {
    difference() {
	cylinder(r=70/2, h=15);
	translate([-22,-20,-epsilon]) {
	    battery();
	    translate([batt_len + 7, 10,0]) rotate([0,0,120]) battery();
	    translate([- 7, 10,0]) rotate([0,0,60]) battery();
	}
    }
}

module item_blocks(negatives=true, offset=0) {
    two_batt_module(negatives=negatives);
    remote_control(negatives=negatives);
}

module bottom(h=bottom_thick, larger=0, fudge=1.246, negatives=false) {
    echo("Bottom diameter", (diameter - 0)/2 - larger);
    //translate([0,0,-3]) color("red") cylinder(r=31.511, h=h);
    // Fudge factor - for some reason, the dxf is completely non-scaled.
    minkowski() {
	rotate([0,0,90]) linear_extrude(height=h) scale(31.511/fudge) translate([-fudge,-fudge]) import(file = "bottom.dxf");
	cylinder(r=larger, h=epsilon);
    }
    if (negatives) {
	// Drill holes through the case
	translate([0, 25, 0]) {
	    cylinder(r=3.4/2, h=20);
	    translate([0,0,11]) m3_bolt(h=12);
	}
	translate([0, -25, 0]) {
	    cylinder(r=3.4/2, h=20);
	    translate([0,0,11]) m3_bolt(h=12);
	}
    }
}

module case(base_high=vertical_divide, bottom_thick=bottom_thick) {
    difference() {
	union() {
	    cylinder(r=diameter/2 - clearance, h=base_high);
	    knurls(h=base_high-knob_overlap-case_knob_clearance);
	}
	translate([0,0,-epsilon]) {
	    translate([0,0,bottom_thick - epsilon]) {
		two_batt_module(negatives=true);
		remote_control(negatives=true);
	    }
 	    bottom(h=bottom_thick, larger=clearance, negatives=true);
	}
    }
}

module knob(hovering=vertical_divide, inner_clearance=2, bottom_thick=bottom_thick) {
    translate([0, 0, hovering - knob_overlap]) difference() {
	// Full knob...
	knurl_knob(total_height - hovering + knob_overlap);

	// Minus interior
	translate([0,0,-epsilon]) cylinder(r=diameter/2 + clearance, h=knob_overlap + inner_clearance);
	translate([0,0,bottom_thick-hovering+knob_overlap]) item_blocks(negatives=true);
    }
}

module xray() {
    difference() {
	union() {
	    case(base_high=vertical_divide);
	    color("red") knob(hovering=vertical_divide);
	    %bottom();
	}
	translate([0,0,-epsilon]) cube([100,100,100]);
    }
}

//bottom(negatives=true);
//battery();
//xray();
//led();
//knob();
//case();
//rotate([180, 0, 0]) knob();
rotate([180,0,0]) case();
//sphere(r=10, $fn=8);
//translate([0,0,bottom_thick]) remote_control();
//translate([-40,0,0]) two_batt_module();
//translate([+40,0,0]) three_batt_module();
