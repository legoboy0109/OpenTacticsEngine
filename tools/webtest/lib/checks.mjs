// Collects PASS/FAIL results of a run and prints them as it goes.
export class Checks {
  constructor() { this.failures = []; this.passes = 0; this.notes = []; }

  check(name, ok, detail = '') {
    if (ok) { this.passes++; console.log(`  PASS ${name}${detail ? ' - ' + detail : ''}`); }
    else { this.failures.push(name); console.log(`  FAIL ${name}${detail ? ' - ' + detail : ''}`); }
    return ok;
  }

  // Informational line (measurements etc.), not a pass/fail.
  note(text) { this.notes.push(text); console.log(`  NOTE ${text}`); }

  // Console errors collected by a Session must be empty.
  noErrors(session, name = 'no console/page errors') {
    return this.check(name, session.errors.length === 0, session.errors.slice(0, 3).join(' | '));
  }

  summary() {
    console.log(`\n${this.passes} passed, ${this.failures.length} failed`);
    for (const f of this.failures) console.log('  failed: ' + f);
    return this.failures.length === 0;
  }
}
