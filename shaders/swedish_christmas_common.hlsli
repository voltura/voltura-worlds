float christmasDaylight(float seconds) {
 // Ease from night to day, then mirror the transition back to night.
 const float legSeconds=15.*60.;
 float phase=fmod(seconds,2.*legSeconds);
 return smoothstep(0.,legSeconds,min(phase,2.*legSeconds-phase));
}
