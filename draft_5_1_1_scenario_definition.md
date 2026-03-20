% ═══════════════════════════════════════════════════════════════════
% Section 5.1.1 — Simulation Environment Configuration
% Cart-OnePiece Thesis — Ericson
% Revised draft: first-person plural, figures, table, GUI deferred to Ch.6
% ═══════════════════════════════════════════════════════════════════

\subsection{Simulation Environment Configuration} \label{subsec:sim_env}

The first stage of the CArt-OnePiece pipeline is responsible for initializing the simulation environment that supports the subsequent stages of the framework. This stage encompasses: (i) the definition of the virtual scenario and the execution of the CARLA server; (ii) the instantiation of the ego vehicle and the configuration of the dynamic actors that populate the scene; and (iii) the definition of the sensor suite attached to the ego vehicle. Figure~\ref{fig:client_server_arch} presents the overall client-server architecture of this stage, illustrating the communication between the CARLA server, the C++ client application, and the traffic manager. The following topics describe each of these steps in detail.

\begin{figure}[H]
  \setlength{\abovecaptionskip}{0pt} \setlength{\belowcaptionskip}{0pt}
  \caption{\MakeUppercase{Client-server architecture of the simulation environment}}
  \centering
  \includegraphics[width=1\textwidth]{imagens/Client_Server_Architecture.png}
  \captionsetup{justification=centering}
  \captionfont{\small{\textbf{\\Source: Elaborated by the author}}}
  \label{fig:client_server_arch}
\end{figure}

% ───────────────────────────────────────────────────────────────────
\subsubsection{Scenario Definition and CARLA Server Execution}
\label{subsubsec:scenario_def}

We built the simulation environment of the CArt-OnePiece framework upon the CARLA simulator \cite{Dosovitskiy2017}, which operates on a client-server architecture. In this model, the CARLA server is responsible for rendering the virtual world, simulating physics, and managing the environmental conditions of the urban scenario, while the client-side application connects to it, issues commands, and extracts the data produced during the simulation. We chose CARLA because it provides an open-source, photorealistic simulation platform with native support for sensor simulation, traffic generation, and synchronous execution, which are fundamental requirements of our framework.

Before any experiment can be conducted, we first execute the CARLA server process. We developed a dedicated shell script that launches the server in offscreen rendering mode and accepts configurable parameters such as the network port and the path to the CARLA installation. This mode allows the simulator to operate without a graphical display, which is essential for running experiments on headless workstations or remote servers. The server process is started asynchronously in the background, and its process identifier is recorded to enable automated lifecycle management, including graceful shutdown and cleanup after each experimental run. Figure~\ref{fig:server_lifecycle} illustrates the complete lifecycle of the server, from launch to cleanup.

\begin{figure}[H]
  \setlength{\abovecaptionskip}{0pt} \setlength{\belowcaptionskip}{0pt}
  \caption{\MakeUppercase{Server lifecycle and scenario configuration flow}}
  \centering
  \includegraphics[width=1\textwidth]{imagens/Server_Lifecycle_Flow.png}
  \captionsetup{justification=centering}
  \captionfont{\small{\textbf{\\Source: Elaborated by the author}}}
  \label{fig:server_lifecycle}
\end{figure}

Once the server is active and ready to accept connections, we proceed to the scenario definition phase. In this step, a client application connects to the CARLA server and specifies the properties that will define the simulated world. The map selection is performed via the CARLA Python API through an external configuration script, while the C++ client application can also issue a map load request as a fallback mechanism. The primary configurable parameter at this point is the selection of the map, which determines the urban layout, road topology, and visual characteristics of the scenario. Currently, the framework supports the standard optimized CARLA towns (Town01\_Opt through Town10HD\_Opt), ranging from small urban layouts to complex multi-lane environments.

Once the map is loaded, we apply a set of simulation parameters to the world. Specifically, we reconfigure the world to operate in synchronous mode with a fixed simulation step, defined by the target frames per second. This configuration guarantees deterministic simulation behavior, ensuring that every sensor reading corresponds to a uniquely identifiable simulation tick, which is a fundamental requirement for reproducible experiments and dataset generation.

Following the map configuration, we configure the traffic conditions of the scenario using the CARLA Traffic Manager. Both the number of vehicles and pedestrians to be spawned in the scene are user-configurable parameters. We spawn these dynamic actors asynchronously using the CARLA Python API, and their behavior is governed by the Traffic Manager, which handles lane following, collision avoidance, and speed regulation. We chose to delegate traffic generation to the Python API because it provides a mature and well-tested interface for actor management, while the core pipeline remains implemented in C++ for performance reasons. This population of dynamic actors ensures that the sensor data and resulting workload reflect a realistic urban traffic scenario rather than an empty static world. Table~\ref{tab:scenario_params} summarizes the configurable parameters available for scenario definition.

\begin{table}[H]
\centering
\caption{\MakeUppercase{Configurable parameters for scenario definition}}
\label{tab:scenario_params}
\begin{tabular}{llll}
\hline
\textbf{Parameter}        & \textbf{Description}                          & \textbf{Default}      & \textbf{Interface} \\
\hline
Map                       & CARLA town layout                             & Town03\_Opt            & Python API         \\
Vehicles                  & Number of NPC vehicles                        & 30                    & Python API         \\
Pedestrians               & Number of NPC pedestrians                     & 10                    & Python API         \\
Port                      & CARLA server RPC port                         & 2000                  & Shell script       \\
FPS                       & Simulation ticks per second                   & 20                    & C++ API            \\
Synchronous mode          & Deterministic tick-locked execution            & Enabled               & C++ API            \\
Rendering mode            & Offscreen (headless) rendering                & Offscreen             & Shell script       \\
\hline
\end{tabular}
\captionfont{\small{\textbf{\\Source: Elaborated by the author}}}
\end{table}

The entire sequence of server execution, map selection, and traffic generation is orchestrated by a single automation script that coordinates all phases and ensures proper cleanup in case of failure (as illustrated in Figure~\ref{fig:server_lifecycle}). Additionally, we developed a graphical user interface using the CustomTkinter library, as shown in the architecture of Figure~\ref{fig:client_server_arch}. Through this interface, users can select from a predefined list of available maps, specify the number of vehicles and pedestrians for traffic generation, define the connection parameters to the CARLA server, and choose the execution mode of the pipeline. This GUI constructs the corresponding command-line arguments and invokes the orchestration script, thereby lowering the barrier to reproducing and customizing simulation scenarios. Detailed usage examples and screenshots of the interface are presented in Chapter~\ref{ch:experiments}.
