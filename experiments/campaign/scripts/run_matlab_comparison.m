% Independent MATLAB image-method check on the common vertical-facade scene.
scriptPath = mfilename('fullpath');
root = fileparts(fileparts(scriptPath));
out = fullfile(root,'05_E6_platforms');
override=getenv('RAYTRACING_PLATFORM_DIR');
if ~isempty(override),out=override;end
w = readtable(fullfile(out,'common_walls.csv'),'TextType','string');
receivers = readtable(fullfile(out,'common_receivers.csv'),'TextType','string');
origin = [1838.748 465];
n = height(w);
vertices = zeros(4*n,3);faces = zeros(2*n,3);
for k=1:n
    a=[w.x1(k)-origin(1),w.y1(k)-origin(2)];
    b=[w.x2(k)-origin(1),w.y2(k)-origin(2)];
    vertices(4*k-3:4*k,:)=[a -25;b -25;b 25;a 25];
    faces(2*k-1:2*k,:)=[4*k-3 4*k-2 4*k-1;4*k-3 4*k-1 4*k];
end
scene = triangulation(faces,vertices);
conductivity = 0.0462*(2.4^0.7822);
pm=propagationModel('raytracing','CoordinateSystem','cartesian','Method','image', ...
    'MaxNumReflections',2,'MaxRelativePathLoss',Inf,'MaxAbsolutePathLoss',314, ...
    'SurfaceMaterial','custom','SurfaceMaterialPermittivity',5.24, ...
    'SurfaceMaterialConductivity',conductivity);
antenna=phased.CustomAntennaElement('FrequencyVector',[2e9 3e9], ...
    'AzimuthAngles',[-180 0 180],'ElevationAngles',[-90 0 90], ...
    'SpecifyPolarizationPattern',true,'HorizontalMagnitudePattern',-300*ones(3), ...
    'HorizontalPhasePattern',zeros(3),'VerticalMagnitudePattern',zeros(3), ...
    'VerticalPhasePattern',zeros(3));
tx=txsite('cartesian','Antenna',antenna,'AntennaPosition',[0;0;1.5], ...
    'TransmitterFrequency',2.4e9,'TransmitterPower',10^((14-30)/10));
positions=[receivers.rx_x-origin(1),receivers.rx_y-origin(2),1.5*ones(height(receivers),1)]';
rx=rxsite('cartesian','Antenna',antenna,'AntennaPosition',positions);
tic;rays=raytrace(tx,rx,pm,'Map',scene);elapsed=toc;
summary=table('Size',[height(receivers) 7], ...
    'VariableTypes',{'string','double','double','double','double','double','double'}, ...
    'VariableNames',{'rx_id','num_paths','power_dbm','incoherent_dbm','rms_ns','r1_paths','r2_paths'});
raw=struct('rx_id',{},'distance_m',{},'delay_s',{},'path_loss_db',{},'phase_rad',{},'reflections',{});
for i=1:height(receivers)
    p=rays{1,i};p=p(arrayfun(@(v)v.PropagationDistance<=500+1e-6,p));
    pl=arrayfun(@(v)v.PathLoss,p);phase=arrayfun(@(v)v.PhaseShift,p);
    delay=arrayfun(@(v)v.PropagationDelay,p);orders=arrayfun(@(v)v.NumInteractions,p);
    if isempty(p)
        power=NaN;incoherent=NaN;rms=0;
    else
        linear=10.^(-pl/10);amplitude=10.^(-pl/20).*exp(1j*phase);
        power=14+20*log10(abs(sum(amplitude)));incoherent=14+10*log10(sum(linear));
        meanDelay=sum(linear.*delay)/sum(linear);rms=1e9*sqrt(sum(linear.*(delay-meanDelay).^2)/sum(linear));
    end
    summary(i,:)={receivers.rx_id(i),numel(p),power,incoherent,rms,sum(orders==1),sum(orders==2)};
    for k=1:numel(p)
        raw(end+1)=struct('rx_id',char(receivers.rx_id(i)),'distance_m',p(k).PropagationDistance, ...
            'delay_s',delay(k),'path_loss_db',pl(k),'phase_rad',phase(k),'reflections',orders(k)); %#ok<SAGROW>
    end
end
writetable(summary,fullfile(out,'matlab_results.csv'));
if ~isempty(raw),writetable(struct2table(raw),fullfile(out,'matlab_paths.csv'));end
info=struct('version',version,'method','image','reflections',2,'diffraction',false, ...
    'frequency_hz',2.4e9,'tx_dbm',14,'relative_permittivity',5.24,'conductivity_sm',conductivity, ...
    'walls',n,'receivers',height(receivers),'elapsed_batch_seconds',elapsed, ...
    'wall_vertical_extent_m',[-25 25],'terminal_height_m',1.5,'ground_and_roofs',false, ...
    'polarization','vertical Z, TE for horizontal propagation; horizontal component -300dB', ...
    'note','Controlled reflection-only comparison; cannot validate RT2D knife-edge D1 or 3D urban accuracy');
fid=fopen(fullfile(out,'matlab_metadata.json'),'w');fprintf(fid,'%s\n',jsonencode(info,PrettyPrint=true));fclose(fid);
disp(summary(1:min(2,height(summary)),:));disp(info);
