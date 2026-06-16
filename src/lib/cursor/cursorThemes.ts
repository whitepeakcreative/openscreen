import type { NativeCursorType } from "@/native/contracts";

/**
 * A single themed cursor image override for one {@link NativeCursorType}.
 *
 * width/height/hotspot are in the same ~32-logical-pixel reference as the built-in
 * PRETTY_NATIVE_CURSOR_ASSETS, so a theme asset matches the default cursor's on-screen
 * size regardless of source PNG resolution. The PNG can be higher-res (e.g. 128x128)
 * and is downscaled at draw time for crisper retina output.
 */
export interface CursorThemeAsset {
	/** Path relative to the public asset root, e.g. "cursors/hello-kitty-watermelon/arrow.png". */
	assetPath: string;
	width: number;
	height: number;
	hotspotX: number;
	hotspotY: number;
}

export interface CursorTheme {
	id: string;
	/** Display label. Proper nouns, so not run through i18n. */
	name: string;
	/** Attribution / origin for the artwork. */
	source?: string;
	/**
	 * Per-cursor-type overrides. Missing types fall back to the built-in default art.
	 * Sweezy packs only ship "arrow" and "pointer".
	 */
	assets: Partial<Record<NativeCursorType, CursorThemeAsset>>;
}

/** Sentinel id for the built-in cursor art (no theme override). */
export const DEFAULT_CURSOR_THEME_ID = "default";

/**
 * Bundled cursor themes. To add a pack: drop arrow.png/pointer.png into
 * public/cursors/<id>/ and add an entry here with hotspots normalized to the
 * 32-logical reference (divide a 128px-pack hotspot by 4). No renderer changes needed.
 */
export const CURSOR_THEMES: readonly CursorTheme[] = [
	{
		id: "sesh",
		name: "Sesh",
		assets: {
			arrow: {
				assetPath: "cursors/sesh/arrow.png",
				width: 32,
				height: 32,
				hotspotX: 1,
				hotspotY: 1,
			},
			pointer: {
				assetPath: "cursors/sesh/pointer.png",
				width: 32,
				height: 32,
				hotspotX: 8,
				hotspotY: 1,
			},
		},
	},
];

/** All selectable theme ids, including the built-in default. */
export const CURSOR_THEME_IDS: ReadonlySet<string> = new Set([
	DEFAULT_CURSOR_THEME_ID,
	...CURSOR_THEMES.map((theme) => theme.id),
]);

/** Returns the theme for `id`, or null for the default / unknown ids. */
export function getCursorTheme(id: string | null | undefined): CursorTheme | null {
	if (!id || id === DEFAULT_CURSOR_THEME_ID) {
		return null;
	}
	return CURSOR_THEMES.find((theme) => theme.id === id) ?? null;
}

/**
 * Normalizes a persisted/incoming theme id to a known value, falling back to the
 * default for anything unrecognized.
 */
export function normalizeCursorThemeId(id: unknown): string {
	return typeof id === "string" && CURSOR_THEME_IDS.has(id) ? id : DEFAULT_CURSOR_THEME_ID;
}
