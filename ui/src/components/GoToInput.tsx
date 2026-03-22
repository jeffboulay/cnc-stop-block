import { useState } from "react";
import { useGoTo } from "../hooks/useCommands";

const MAX_TRAVEL = 2000;

export function GoToInput() {
  const [value, setValue] = useState("");
  const [error, setError] = useState("");
  const goTo = useGoTo();

  const handleGo = () => {
    const num = parseFloat(value);

    if (value.trim() === "") {
      setError("Enter a position");
      return;
    }
    if (isNaN(num) || !isFinite(num)) {
      setError("Must be a number");
      return;
    }
    if (num < 0 || num > MAX_TRAVEL) {
      setError(`Range: 0 – ${MAX_TRAVEL} mm`);
      return;
    }

    setError("");
    goTo.mutate(
      { position_mm: num },
      {
        onSuccess: () => setValue(""),
        onError: (e) =>
          setError(e instanceof Error ? e.message : "Command failed"),
      },
    );
  };

  const isPending = goTo.isPending;
  const hasError = !!error;

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
      <div style={{ display: "flex", gap: 8 }}>
        <input
          type="number"
          placeholder="Enter position (mm)"
          step="0.1"
          min="0"
          max={MAX_TRAVEL}
          value={value}
          onChange={(e) => {
            setValue(e.target.value);
            setError("");
          }}
          onKeyDown={(e) => e.key === "Enter" && handleGo()}
          style={{
            flex: 1,
            padding: "12px 16px",
            border: `2px solid ${hasError ? "var(--red)" : "rgba(255,255,255,0.15)"}`,
            borderRadius: "var(--btn-radius)",
            background: "var(--bg-secondary)",
            color: "var(--text-bright)",
            fontSize: 20,
            fontFamily: "'Courier New', monospace",
            minHeight: "var(--touch-min)",
            transition: "border-color 0.15s",
          }}
        />
        <button
          onClick={handleGo}
          disabled={isPending}
          style={{
            padding: "12px 24px",
            borderRadius: "var(--btn-radius)",
            background: isPending ? "rgba(68,138,255,0.5)" : "var(--blue)",
            color: "white",
            fontSize: 18,
            fontWeight: 700,
            minHeight: "var(--touch-min)",
            minWidth: 100,
            opacity: isPending ? 0.7 : 1,
            cursor: isPending ? "not-allowed" : "pointer",
          }}
        >
          {isPending ? "…" : "GO TO"}
        </button>
      </div>
      {hasError && (
        <span style={{ fontSize: 12, color: "var(--red)", paddingLeft: 4 }}>
          ⚠ {error}
        </span>
      )}
    </div>
  );
}
