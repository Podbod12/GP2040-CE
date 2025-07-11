export const ANIMATION_NON_PRESSED_EFFECTS = {
	NONPRESSED_EFFECT_STATIC_COLOR: 0,
	NONPRESSED_EFFECT_RAINBOW_SYNCED: 1,
	NONPRESSED_EFFECT_RAINBOW_ROTATE: 2,
	NONPRESSED_EFFECT_CHASE_SEQUENTIAL: 3,
	NONPRESSED_EFFECT_CHASE_LEFT_TO_RIGHT: 4,
	NONPRESSED_EFFECT_CHASE_RIGHT_TO_LEFT: 5,
	NONPRESSED_EFFECT_CHASE_TOP_TO_BOTTOM: 6,
	NONPRESSED_EFFECT_CHASE_BOTTOM_TO_TOP: 7,
	NONPRESSED_EFFECT_CHASE_SEQUENTIAL_PINGPONG: 8,
	NONPRESSED_EFFECT_CHASE_HORIZONTAL_PINGPONG: 9,
	NONPRESSED_EFFECT_CHASE_VERTICAL_PINGPONG: 10,
	NONPRESSED_EFFECT_CHASE_RANDOM: 11,
	NONPRESSED_EFFECT_JIGGLESTATIC: 12,
	NONPRESSED_EFFECT_JIGGLETWOSTATIC: 13,
	NONPRESSED_EFFECT_RAIN_LOW: 14,
	NONPRESSED_EFFECT_RAIN_MEDIUM: 15,
	NONPRESSED_EFFECT_RAIN_HIGH: 16,
} as const;

export const ANIMATION_PRESSED_EFFECTS = {
	PRESSED_EFFECT_STATIC_COLOR: 0,
	PRESSED_EFFECT_RANDOM: 1,
	PRESSED_EFFECT_JIGGLESTATIC: 2,
	PRESSED_EFFECT_JIGGLETWOSTATIC: 3,
	PRESSED_EFFECT_BURST: 4,
	PRESSED_EFFECT_BURST_RANDOM: 5,
	PRESSED_EFFECT_BURST_SMALL: 6,
	PRESSED_EFFECT_BURST_SMALL_RANDOM: 7,
} as const;
