/**
 * Maps firmware error_code values to user-friendly messages.
 *
 * Codes are emitted by firmware/src/WebAPI.cpp::buildStatusJSON().
 * Keep in sync with the startsWith() checks there whenever new error
 * paths are added to SystemController.cpp.
 */

export interface ErrorMessage {
  /** Short title shown prominently in the ErrorBanner. */
  title: string;
  /** One-sentence explanation shown below the title. */
  detail: string;
}

const ERROR_MESSAGES: Record<string, ErrorMessage> = {
  HOMING_FAILED: {
    title: 'Homing failed',
    detail:
      'The home limit switch was not found. Check that the switch is wired and unobstructed, then press RESET to try again.',
  },
  FAR_LIMIT_TRIGGERED: {
    title: 'Travel limit reached',
    detail:
      'The stop block hit the far end limit switch. Check for obstructions beyond the expected travel range.',
  },
  MOVE_TIMEOUT: {
    title: 'Move timed out',
    detail:
      'The stop block took too long to reach position. Check for obstructions or belt tension issues, then press RESET.',
  },
  POSITION_ERROR: {
    title: 'Position accuracy error',
    detail:
      'The block did not reach the target within tolerance. Check for mechanical slop or a slipping belt, then press RESET.',
  },
  // ESTOP is a firmware state, not an error_code — handled separately in ErrorBanner
  ESTOP: {
    title: 'Emergency stop activated',
    detail:
      'E-Stop was triggered. Clear any obstruction, then press RESET to re-home the machine.',
  },
  UNKNOWN: {
    title: 'Firmware error',
    detail: 'An unexpected error occurred. Press RESET to re-home and try again.',
  },
};

const FALLBACK: ErrorMessage = ERROR_MESSAGES.UNKNOWN!;

/**
 * Returns the user-facing ErrorMessage for a given firmware error_code.
 * Falls back to a generic message for unknown or missing codes.
 *
 * @param code  The error_code string from SystemStatus, or undefined.
 */
export function getErrorMessage(code: string | undefined): ErrorMessage {
  if (!code) return FALLBACK;
  return ERROR_MESSAGES[code] ?? FALLBACK;
}
