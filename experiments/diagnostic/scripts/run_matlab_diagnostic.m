% Recover public comm.Ray.Interactions and map points to finite input facades.
root=fileparts(fileparts(mfilename('fullpath')));
rxfile=getenv('M50_RX_FILE');if isempty(rxfile),rxfile=fullfile(root,'inputs','receivers_initial.csv');end
caseNames=split(string(getenv('M50_SCENES')),',');
if all(strlength(caseNames)==0),caseNames=["A_original";"B_e27_flip";"C_all_flip";"D_two_surfaces";"E_two_buildings"];end
receivers=readtable(rxfile,'TextType','string');[~,rxTag]=fileparts(rxfile);
sigma=.0462*2.4^.7822;origin=[1838.748;465;0];
pm=propagationModel('raytracing','CoordinateSystem','cartesian','Method','image','MaxNumReflections',2,'MaxRelativePathLoss',Inf,'MaxAbsolutePathLoss',314,'SurfaceMaterial','custom','SurfaceMaterialPermittivity',5.24,'SurfaceMaterialConductivity',sigma);
ant=phased.CustomAntennaElement('FrequencyVector',[2e9 3e9],'AzimuthAngles',[-180 0 180],'ElevationAngles',[-90 0 90],'SpecifyPolarizationPattern',true,'HorizontalMagnitudePattern',-300*ones(3),'HorizontalPhasePattern',zeros(3),'VerticalMagnitudePattern',zeros(3),'VerticalPhasePattern',zeros(3));
tx=txsite('cartesian','Antenna',ant,'AntennaPosition',[0;0;1.5],'TransmitterFrequency',2.4e9,'TransmitterPower',10^((14-30)/10));
rx=rxsite('cartesian','Antenna',ant,'AntennaPosition',[receivers.rx_x'-origin(1);receivers.rx_y'-origin(2);1.5*ones(1,height(receivers))]);
for caseIndex=1:numel(caseNames)
    name=char(caseNames(caseIndex));sceneDir=fullfile(root,'inputs','scenes',name);out=fullfile(root,'results','matlab',[name '_' rxTag]);if ~isfolder(out),mkdir(out);end
    w=readtable(fullfile(sceneDir,'walls.csv'),'TextType','string');v=readtable(fullfile(sceneDir,'vertices.csv'));tri=readtable(fullfile(sceneDir,'triangles.csv'),'TextType','string');
    scene=triangulation([tri.i0 tri.i1 tri.i2]+1,[v.x v.y v.z]);
    tic;rays=raytrace(tx,rx,pm,'Map',scene);elapsed=toc;
    summary=struct('rx_id',{},'rx_x',{},'rx_y',{},'num_paths',{},'target_count',{},'target_distance_m',{},'target_individual_dbm',{},'coherent_dbm',{},'incoherent_dbm',{},'rms_ns',{});
    raw=struct('rx_id',{},'path_index',{},'distance_m',{},'path_loss_db',{},'phase_rad',{},'power_dbm',{},'surface_sequence',{},'interaction_positions',{},'interaction_types',{},'facade_mapping_ambiguity',{},'mapping_min_distance_m',{},'mapping_second_distance_m',{},'target',{});
    for i=1:height(receivers)
        p=rays{1,i};p=p(arrayfun(@(a)a.PropagationDistance<=500+1e-6,p));tc=0;td=NaN;tp=NaN;
        for k=1:numel(p)
            ints=p(k).Interactions;if p(k).LineOfSight,ints=[];end
            locs=zeros(numel(ints),3);ids=strings(1,numel(ints));types=strings(1,numel(ints));ambiguous=false;mappingMin=zeros(1,numel(ints));mappingSecond=Inf(1,numel(ints));
            for q=1:numel(ints)
                loc=double(ints(q).Location(:))+origin;locs(q,:)=loc';types(q)=string(ints(q).Type);
                dx=w.x2-w.x1;dy=w.y2-w.y1;u=((loc(1)-w.x1).*dx+(loc(2)-w.y1).*dy)./(dx.^2+dy.^2);uc=max(0,min(1,u));dist=hypot(loc(1)-w.x1-uc.*dx,loc(2)-w.y1-uc.*dy);
                sortedDist=sort(dist);mappingMin(q)=sortedDist(1);if numel(sortedDist)>1,mappingSecond(q)=sortedDist(2);end
                matches=find(dist<=1e-4 & u>=-1e-7 & u<=1+1e-7 & loc(3)>=-25-1e-4 & loc(3)<=25+1e-4);
                if numel(matches)==1,ids(q)=w.id(matches);else,ids(q)=strjoin(w.id(matches),'|');ambiguous=true;end
            end
            target=numel(ids)==2 && ~ambiguous && ids(1)=="osm_556278505_e0" && ids(2)=="osm_419317355_e30" && all(types=="Reflection");
            if target,tc=tc+1;td=p(k).PropagationDistance;tp=14-p(k).PathLoss;end
            raw(end+1)=struct('rx_id',char(receivers.rx_id(i)),'path_index',k-1,'distance_m',p(k).PropagationDistance,'path_loss_db',p(k).PathLoss,'phase_rad',p(k).PhaseShift,'power_dbm',14-p(k).PathLoss,'surface_sequence',{cellstr(ids)},'interaction_positions',locs,'interaction_types',{cellstr(types)},'facade_mapping_ambiguity',ambiguous,'mapping_min_distance_m',mappingMin,'mapping_second_distance_m',mappingSecond,'target',target); %#ok<SAGROW>
        end
        pl=arrayfun(@(a)a.PathLoss,p);ph=arrayfun(@(a)a.PhaseShift,p);delay=arrayfun(@(a)a.PropagationDelay,p);
        if isempty(p),power=NaN;inc=NaN;rms=NaN;else,weights=10.^(-pl/10);power=14+20*log10(abs(sum(10.^(-pl/20).*exp(1j*ph))));inc=14+10*log10(sum(weights));meanDelay=sum(weights.*delay)/sum(weights);rms=1e9*sqrt(sum(weights.*(delay-meanDelay).^2)/sum(weights));end
        summary(end+1)=struct('rx_id',char(receivers.rx_id(i)),'rx_x',receivers.rx_x(i),'rx_y',receivers.rx_y(i),'num_paths',numel(p),'target_count',tc,'target_distance_m',td,'target_individual_dbm',tp,'coherent_dbm',power,'incoherent_dbm',inc,'rms_ns',rms); %#ok<SAGROW>
    end
    writetable(struct2table(summary),fullfile(out,'summary.csv'));
    f=fopen(fullfile(out,'paths.json'),'w');fprintf(f,'%s\n',jsonencode(raw,PrettyPrint=true));fclose(f);
    save(fullfile(out,'raw_rays.mat'),'rays','scene','w','receivers');
    info=struct('engine','MATLAB','version',version,'scene',name,'receivers',height(receivers),'rx_file',rxfile,'seconds',elapsed,'API','comm.Ray.Interactions(q).Location/Type; unique finite-facade mapping <=1e-4 m, recording actual and second-nearest distances; no public primitive index','frequency_hz',2.4e9,'tx_dbm',14,'polarization','V/TE','eps_r',5.24,'sigma_Sm',sigma,'z_facades_m',[-25 25],'max_reflections',2,'max_path_m',500,'max_absolute_pathloss_db',314);
    f=fopen(fullfile(out,'metadata.json'),'w');fprintf(f,'%s\n',jsonencode(info,PrettyPrint=true));fclose(f);
    fprintf('%s: %d RX; target present %d\n',name,height(receivers),sum([summary.target_count]>0));
end
