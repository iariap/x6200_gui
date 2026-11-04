# Riesgo residual
1. Si el árbitro/transporte falla con RX_LINUX seleccionado, la radio podría quedar en un estado de
   audio no confirmado; software no puede garantizar retorno BASE sin readback ni segunda vía. Mitigación
   actual: RX_LINUX sólo QA y fail-closed; no se activa en producción.
2. Un partial/failed write puede dejar estado incierto; la sombra se invalida y no se reutiliza, pero la
   radio no se puede auditar sin acceso físico/readback.
3. Un reboot no permite conocer el estado MUX inicial; no hay garantía software de BASE.
4. El watchdog/supervisor no es independiente del árbitro/transporte; no debe presentarse como tal.
5. La evidencia acústica (si se ejecuta el protocolo) prueba BASE audible, no selección de registro/MUX.
6. Cualquier sesión activa requiere autorización específica del owner y método probado de BASE return.
