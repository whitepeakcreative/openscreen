import { Plus, X } from "lucide-react";
import { useState } from "react";
import { v4 as uuidv4 } from "uuid";
import { Slider } from "@/components/ui/slider";
import { cn } from "@/lib/utils";

interface ColorStop {
	id: string;
	color: string;
	position: number;
}

interface CustomGradientBuilderProps {
	onApply: (css: string) => void;
	onCancel: () => void;
}

function buildCss(type: "linear" | "radial", angle: number, stops: ColorStop[]): string {
	const sorted = [...stops].sort((a, b) => a.position - b.position);
	const stopStr = sorted.map((s) => `${s.color} ${s.position}%`).join(", ");
	return type === "radial"
		? `radial-gradient(circle, ${stopStr})`
		: `linear-gradient(${angle}deg, ${stopStr})`;
}

export default function CustomGradientBuilder({ onApply, onCancel }: CustomGradientBuilderProps) {
	const [type, setType] = useState<"linear" | "radial">("linear");
	const [angle, setAngle] = useState(135);
	const [stops, setStops] = useState<ColorStop[]>([
		{ id: uuidv4(), color: "#6366f1", position: 0 },
		{ id: uuidv4(), color: "#ec4899", position: 100 },
	]);

	const previewCss = buildCss(type, angle, stops);

	const addStop = () => {
		if (stops.length >= 5) return;
		setStops((prev) =>
			[...prev, { id: uuidv4(), color: "#ffffff", position: 50 }].sort(
				(a, b) => a.position - b.position,
			),
		);
	};

	const removeStop = (id: string) => {
		if (stops.length <= 2) return;
		setStops((prev) => prev.filter((s) => s.id !== id));
	};

	const updateStop = (id: string, update: Partial<Omit<ColorStop, "id">>) => {
		setStops((prev) => prev.map((s) => (s.id === id ? { ...s, ...update } : s)));
	};

	return (
		<div className="mt-2 rounded-lg border border-white/10 bg-[#1a1a1d] overflow-hidden">
			<div className="h-8 w-full transition-all duration-200" style={{ background: previewCss }} />

			<div className="p-3 space-y-3">
				<div className="flex gap-1">
					{(["linear", "radial"] as const).map((t) => (
						<button
							key={t}
							type="button"
							className={cn(
								"flex-1 text-[10px] font-semibold py-1 rounded-md capitalize transition-colors",
								type === t
									? "bg-[#34B27B] text-white"
									: "bg-white/5 text-slate-400 hover:text-white",
							)}
							onClick={() => setType(t)}
						>
							{t}
						</button>
					))}
				</div>

				{type === "linear" && (
					<div className="flex items-center gap-2">
						<span className="text-[10px] text-slate-400 w-10 shrink-0">Angle</span>
						<Slider
							min={0}
							max={360}
							step={1}
							value={[angle]}
							onValueChange={([v]) => setAngle(v)}
							className="flex-1"
						/>
						<span className="text-[10px] text-slate-300 w-8 text-right">{angle}°</span>
					</div>
				)}

				<div className="space-y-2">
					<div className="flex items-center justify-between">
						<span className="text-[10px] font-semibold text-slate-400 uppercase tracking-widest">
							Stops
						</span>
						{stops.length < 5 && (
							<button
								type="button"
								onClick={addStop}
								className="text-[10px] text-[#34B27B] hover:text-[#34B27B]/80 flex items-center gap-0.5 transition-colors"
							>
								<Plus className="w-3 h-3" /> Add
							</button>
						)}
					</div>
					{stops.map((stop) => (
						<div key={stop.id} className="flex items-center gap-2">
							<input
								type="color"
								value={stop.color}
								onChange={(e) => updateStop(stop.id, { color: e.target.value })}
								className="w-7 h-7 rounded cursor-pointer border border-white/10 bg-transparent p-0.5"
							/>
							<Slider
								min={0}
								max={100}
								step={1}
								value={[stop.position]}
								onValueChange={([v]) => updateStop(stop.id, { position: v })}
								className="flex-1"
							/>
							<span className="text-[10px] text-slate-400 w-7 text-right shrink-0">
								{stop.position}%
							</span>
							<button
								type="button"
								onClick={() => removeStop(stop.id)}
								disabled={stops.length <= 2}
								className="text-slate-600 hover:text-red-400 disabled:opacity-30 disabled:cursor-not-allowed transition-colors"
							>
								<X className="w-3 h-3" />
							</button>
						</div>
					))}
				</div>

				<div className="flex gap-1.5 pt-1">
					<button
						type="button"
						onClick={onCancel}
						className="flex-1 text-[10px] font-semibold py-1.5 rounded-md bg-white/5 text-slate-400 hover:text-white transition-colors"
					>
						Cancel
					</button>
					<button
						type="button"
						onClick={() => onApply(previewCss)}
						className="flex-1 text-[10px] font-semibold py-1.5 rounded-md bg-[#34B27B] text-white hover:bg-[#34B27B]/90 transition-colors"
					>
						Apply
					</button>
				</div>
			</div>
		</div>
	);
}
