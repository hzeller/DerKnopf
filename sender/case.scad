include <knob.scad>

$fn=180;
epsilon=0.02;
clearance = 0.35;
pcb_width=14;
pcb_distance=1;
diameter=65;   // TODO: calculate.
batt_len=44.5 + 6 + 2*clearance;  // Additional space for contacts
batt_dia=10.5 + 2*clearance;
font="Helvetica:style=Bold";
total_height=28;
vertical_divide=15;
knob_overlap=6;
bottom_thick=3.2;  // acrylic

module battery(length=batt_len, dia=batt_dia,negatives=true) {
    translate([-batt_len/2,0,0]) {
	translate([0,0,dia/2]) rotate([0,90,0]) cylinder(r=dia/2, h=length);
	translate([0,-dia/2,0]) cube([length, dia, dia/2]);
	if (negatives) {
	    // Contact blades
	    translate([0,-0.6*dia,1/8*dia]) cube([1, 1.4*dia, 3/4 * dia]);
	    translate([batt_len-1,-0.6*dia,1/8*dia]) cube([1, 1.4*dia, 3/4 * dia]);
	    
	    translate([0,-0.5,dia/4]) rotate([90,0,0]) translate([0,0,-dia/2]) {
		translate([5,-6/2+1,0]) color("black") linear_extrude(height=dia + 0.4) text("–", size=8, font=font);
		translate([length-10,-6/2+1,0]) color("red") linear_extrude(height=dia + 0.4) text("+", size=8, font=font);
	    }
	}
    }
}

module remote_control(l=37, w=pcb_width, h=9,negatives=true) {
    translate([-l/2,-w/2,0]) cube([l,w,h]);
    if (negatives) color("lightblue") {
	difference() {    // turn shaft
	    cylinder(r=6.1/2, h=h + 14.55);
	    translate([-4, 1.5, -epsilon]) cube([8, 8, 25+2*epsilon]);
	}
	cylinder(r=9/2 + clearance, h=h + 6.8);  // mount shaft

	// madenschraube
	translate([0,0,h + 10]) rotate([-90, 0, 0]) union() {
	    cylinder(r=3.2/2, h=50);
	    translate([0,0,3]) {
		// Space for bolt
		rotate([0,0,30]) cylinder(r=5.7/2, h=2.6, $fn=6);
		translate([-5.5/2,0,0]) cube([5.5, 6, 2.6]);
	    }
	    translate([0,0,9.5]) {
		cylinder(r=6/2, h=12);
		translate([0,0,12-4]) translate([-6/2,0,0]) cube([6, 7, 4]);
		translate([-3/2,0,0]) cube([3, 7, 12]);
	    }
	}
	translate([0,0,h + 1.2]) cylinder(r=15/2 + clearance, h=3);  // mount gear
	// Punch-through for cables.
	translate([0,0,h-1.5-epsilon]) cube([l+15, 4, 3], center=true);
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

module bottom(h=bottom_thick, smaller=0) {
    cylinder(r=(diameter - 0)/2 - smaller, h=h - smaller);
}

module case(base_high=vertical_divide, bottom_thick=bottom_thick) {
    difference() {
	union() {
	    cylinder(r=diameter/2 - clearance, h=base_high);
	    knurls(h=base_high-knob_overlap);
	}
	translate([0,0,-epsilon]) {
	    translate([0,0,bottom_thick - epsilon]) {
		two_batt_module(negatives=true);
		remote_control(negatives=true);
	    }
	    bottom(h=bottom_thick);
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
	    color("red") knob(hovering=vertical_divide+clearance);
	    %bottom(smaller=clearance);
	}
	translate([0,0,-epsilon]) cube([100,100,100]);
    }
}

//battery();
//case();
xray();
//knob();
//rotate([180, 0, 0]) knob();
//sphere(r=10, $fn=8);
//remote_control();
//translate([-40,0,0]) two_batt_module();
//translate([+40,0,0]) three_batt_module();
