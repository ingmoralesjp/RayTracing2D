% Directed inspection of installed image-method candidate visibility.
% Calls vendor functions without editing or redistributing their source.
root=fileparts(fileparts(mfilename('fullpath')));out=fullfile(root,'results','matlab_visibility');mkdir(out);
sceneNames=["A_original","B_e27_flip","C_all_flip","N_e30_reverse","N_outward","R_e30_refine100","R_e30_refine1000"];
inputRX=readtable(fullfile(root,'inputs','receivers_initial.csv'),'TextType','string');inputRX=inputRX(1,:);
audit=jsondecode(fileread(fullfile(root,'independent_audit','receivers_initial.json')));pts=audit.positions(1).candidate_points;origin=[1838.748 465 0];tx=[0 0 1.5];rx=[1950-1838.748 35 1.5];p1=[pts(2,:) 1.5]-origin;p2=[pts(3,:) 1.5]-origin;
results=struct('scene',{},'facet_id',{},'surface',{},'contains_P2',{},'in_RX_viewshed',{},'normal_dot_RX',{},'sample_kind',{},'sample_xyz',{},'sample_visible',{},'first_blocker_wall',{},'first_blocker_facet',{});meta=struct([]);
for sceneIndex=1:numel(sceneNames)
    label=char(sceneNames(sceneIndex));folder=fullfile(root,'inputs','scenes',label);v=readtable(fullfile(folder,'vertices.csv'));f=readtable(fullfile(folder,'triangles.csv'),'TextType','string');tri=triangulation([f.i0 f.i1 f.i2]+1,[v.x v.y v.z]);rt=matlabshared.internal.StaticSceneRayTracer(tri);env=struct('Triangulation',tri,'RayTracer',rt,'SharpEdgeFlags',comm.internal.geometry.getSharpEdges(tri));
    visTX=comm.internal.geometry.viewshed(tri,rt,tx);visRX=comm.internal.geometry.viewshed(tri,rt,rx);targetFacets=find(f.wall_id=="osm_419317355_e30");firstFacets=find(f.wall_id=="osm_556278505_e0");
    [ref,refIDs]=comm.internal.geometry.calcImageReflectionPath(env,tx,visTX{1},rx,2,2);
    refSeq=strings(size(refIDs));for jj=1:numel(refIDs),refSeq(jj)=f.wall_id(refIDs(jj));end
    targetWithoutChange=any(all(refSeq==["osm_556278505_e0","osm_419317355_e30"],2));
    for facet=targetFacets'
        xyz=tri.Points(tri.ConnectivityList(facet,:),:);vv=xyz(2,:)-xyz(1,:);ww=xyz(3,:)-xyz(1,:);q=p2-xyz(1,:);bary=[dot(vv,vv) dot(vv,ww);dot(vv,ww) dot(ww,ww)]\[dot(q,vv);dot(q,ww)];inside=all(bary>=-1e-8)&&sum(bary)<=1+1e-8;
        normal=faceNormal(tri,facet);samplePoints=[incenter(tri,facet);xyz;p2];sampleKinds=["incenter","vertex1","vertex2","vertex3","actual_P2"];
        for k=1:size(samplePoints,1)
            sample=samplePoints(k,:);[~,blocker,blocked]=comm.internal.geometry.firstIntersect(env,rx,sample,'segment');blockerWall="";if ~isnan(blocker),blockerWall=f.wall_id(blocker);end
            results(end+1)=struct('scene',label,'facet_id',facet-1,'surface','osm_419317355_e30','contains_P2',inside,'in_RX_viewshed',ismember(facet,visRX{1}),'normal_dot_RX',dot(normal,rx-incenter(tri,facet)),'sample_kind',char(sampleKinds(k)),'sample_xyz',sample+origin,'sample_visible',~blocked,'first_blocker_wall',char(blockerWall),'first_blocker_facet',blocker-1); %#ok<SAGROW>
        end
    end
    % All three physical legs checked through the installed intersection API.
    [~,~,blocked1]=comm.internal.geometry.firstIntersect(env,tx,p1,'segment');[~,~,blocked2]=comm.internal.geometry.firstIntersect(env,p1,p2,'segment');[~,~,blocked3]=comm.internal.geometry.firstIntersect(env,p2,rx,'segment');
    % Supply the known last-reflector facets directly to the existing
    % order-one candidate routine. This is instrumentation, not a new solver.
    image1=comm.internal.geometry.mirror(tri,tx,firstFacets(1));
    [lastPts,lastFacets]=comm.internal.geometry.calcImageReflectionPath(env,image1,targetFacets,rx,1,2);
    explicitR2=zeros(0,3,2);explicitIDs=zeros(0,2);
    if ~isempty(lastPts)
        [pFirst,firstIDs]=comm.internal.geometry.firstIntersect(env,lastPts(:,:,1),image1,'segment',true);
        for k=1:numel(firstIDs)
            if ~isnan(firstIDs(k)) && ismember(firstIDs(k),firstFacets)
                [~,~,blocked]=comm.internal.geometry.firstIntersect(env,tx,pFirst(k,:),'segment');
                if ~blocked,explicitR2(end+1,:,:)=cat(3,pFirst(k,:),lastPts(k,:,1));explicitIDs(end+1,:)=[firstIDs(k) lastFacets(k)];end %#ok<SAGROW>
            end
        end
    end
    m=struct('scene',label,'installed_image_pipeline_target_found',targetWithoutChange,'installed_image_paths',size(refIDs,1),'target_first_facets_in_TX_viewshed',intersect(firstFacets,visTX{1})'-1,'target_last_facets_in_RX_viewshed',intersect(targetFacets,visRX{1})'-1,'three_actual_legs_occluded',[blocked1 blocked2 blocked3],'explicit_last_facade_candidate_count',size(lastPts,1),'explicit_last_facade_candidate_positions',lastPts,'explicit_last_facade_candidate_ids',lastFacets-1,'explicit_R2_sequence_reconstructed',~isempty(explicitIDs),'explicit_R2_points',explicitR2,'explicit_R2_facets',explicitIDs-1);
    if isempty(meta),meta=m;else,meta(end+1)=m;end %#ok<SAGROW>
end
fid=fopen(fullfile(out,'facet_visibility.json'),'w');fprintf(fid,'%s\n',jsonencode(results,PrettyPrint=true));fclose(fid);fid=fopen(fullfile(out,'pipeline_diagnostics.json'),'w');fprintf(fid,'%s\n',jsonencode(meta,PrettyPrint=true));fclose(fid);
sourceNames={'comm.internal.geometry.viewshed','comm.internal.geometry.calcImageReflectionPath','comm.internal.geometry.firstIntersect'};sources=struct('symbol',{},'installed_path',{});for i=1:numel(sourceNames),sources(end+1)=struct('symbol',sourceNames{i},'installed_path',which(sourceNames{i}));end
fid=fopen(fullfile(out,'installed_API_sources.json'),'w');fprintf(fid,'%s\n',jsonencode(sources,PrettyPrint=true));fclose(fid);disp(struct2table(meta));
