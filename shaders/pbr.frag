#version 450

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 worldTangent;

layout(location = 0) out vec4 outColor;

layout(constant_id = 0) const int BRDF = 0;

layout(set = 0, binding = 1) uniform MaterialUBO {
    vec3 albedo;
    float metallic;
    float roughness;
    
    float sheen;
    float sheenTint;
    float clearcoat;
    float clearcoatGloss;    
} material;

layout(set = 0, binding = 2) uniform sampler2D tex;
layout(set = 0, binding = 3) uniform sampler2D normalMap;
layout(set = 0, binding = 4) uniform sampler2D roughnessMap;
layout(set = 0, binding = 5) uniform sampler2D metallicMap;

layout(set = 1, binding = 0) uniform SceneUBO {
    bool ibl;
    vec4 lightPos;
    vec3 lightColor;
    vec3 ambientLight;

    vec3 camPos;

    bool toneMapping;
    float exposure;

    int textures;
} scene;

layout(set = 2, binding = 0) uniform samplerCube irradiance;
layout(set = 2, binding = 1) uniform samplerCube prefilter;
layout(set = 2, binding = 2) uniform sampler2D brdfLUT;

#define BRDF_DIFFUSE_NONE           (1 << 0)
#define BRDF_DIFFUSE_LAMBERT        (1 << 1)
#define BRDF_DIFFUSE_OREN_NAYAR     (1 << 2)
#define BRDF_DIFFUSE_BURLEY         (1 << 3)

#define BRDF_SPECULAR_NONE          (1 << 4)
#define BRDF_SPECULAR_BLINN_PHONG   (1 << 5)
#define BRDF_SPECULAR_COOK_TORRANCE (1 << 6)
#define BRDF_SPECULAR_WARD          (1 << 7)
#define BRDF_SPECULAR_DISNEY        (1 << 8)

#define TEXTURE_ALBEDO              (1 << 0)
#define TEXTURE_NORMAL_MAP          (1 << 1)
#define TEXTURE_ROUGHNESS_MAP       (1 << 2)
#define TEXTURE_METALLIC_MAP        (1 << 3)

const float PI = 3.1415926535;

vec3 samplePrefilter(vec3 n, float roughness) {
    float lod = roughness * 11.0f;
    lod = min(9.0f, lod);
    float lod0 = floor(lod);
    float lod1 = min(lod0 + 1.0, 9.0f);
    float t = fract(lod);

    vec3 a = textureLod(prefilter, n, lod0).rgb;
    vec3 b = textureLod(prefilter, n, lod1).rgb;

    return mix(a, b, t);

}
vec3 F_schlick(vec3 v, vec3 h, float metallic, vec3 albedo) {
    float vdoth = max(dot(v,h), 0.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    return F0 + (1.0 - F0) * pow(1.0 - vdoth, 5.0);
}

float G1(vec3 v, vec3 n, float roughness) {
    float k = pow((roughness + 1), 2) / 8;
    float ndotv = max(dot(normalize(n), v), 0);
    return max(ndotv / (ndotv * (1-k) + k), 0);
}

float G_Smith(vec3 l, vec3 v, vec3 n, float roughness) {
    return max(G1(v, n, roughness) * G1(l, n, roughness), 0);
}

float D_GGX(vec3 h, vec3 n, float roughness) {
    float ndoth = max(dot(normalize(n), h), 0);
    float alpha2 = pow(roughness, 4);
    return alpha2/max(PI * pow(ndoth*ndoth * (alpha2-1) + 1, 2), 0.00000001);
}

float D_GTR1(float ndoth, float alpha) {
    float alpha2 = alpha * alpha;
    float cos2 = ndoth * ndoth;
    float denom = PI * log(alpha2) * ((alpha2*cos2) + (1-cos2));
    return (alpha2 - 1) / denom;
}

vec3 f_clearcoat(vec3 N, vec3 V, vec3 L, vec3 H, float roughness) {
    float ndotl = max(dot(N,L), 0.0);
    float ndotv = max(dot(N,V), 0.0);
    float ndoth = max(dot(N,H), 0.0);
    float vdoth = max(dot(V,H), 0.0);
    float denom = max(4*ndotl*ndotv, 0.00000001);
    float alpha = mix(0.1, 0.001, material.clearcoatGloss);
    vec3 F = vec3(0.04 + (1.0 - 0.04) * pow(1.0 - vdoth, 5.0));
    return material.clearcoat * D_GTR1(ndoth, alpha) * F * G_Smith(L, V, N, roughness) * ndotl / denom;
}

vec3 f_sheen(vec3 N, vec3 L, vec3 H, vec3 albedo) {
    float ldoth = max(dot(L, H), 0.0);
    float ndotl = max(dot(N, L), 0.0);

    if (ndotl <= 0.0)
        return vec3(0.0);

    float F = pow(1.0 - ldoth, 5.0);

    vec3 tint = albedo /
        max(max(albedo.r, albedo.g), albedo.b);

    vec3 sheenColor = mix(vec3(1.0), tint, material.sheenTint);

    return material.sheen * sheenColor * F * ndotl;
}

vec3 fs_CookTorrance(vec3 v, vec3 l, vec3 h, vec3 n, vec3 albedo, float metallic, float roughness) {
    float ndotl = dot(normalize(n), l);
    float ndotv = dot(normalize(n), v);
    return (D_GGX(h, n, roughness) * F_schlick(v, h, metallic, albedo) * G_Smith(l, v, n, roughness)) / max(4 * ndotl * ndotv, 0.00000001);
}


float fs_blinnphong(vec3 h, vec3 n, float roughness) {
    float ndoth = max(dot(n, h), 0.0);
    float shininess = pow(2.0, mix(1.0, 10.0, 1.0 - roughness));
    return (shininess + 2) * (0.5/PI) * pow(ndoth, shininess);
}

float fs_ward(
    vec3 N,
    vec3 V,
    vec3 L,
    vec3 T,
    vec3 B,
    float alphaX,
    float alphaY
) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    if (NdotL <= 0.0 || NdotV <= 0.0)
        return 0.0;

    vec3 H = normalize(L + V);

    float NdotH = max(dot(N, H), 1e-6);
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);

    float tanThetaH2 = (1.0 - NdotH * NdotH) / (NdotH * NdotH);

    float exponent =
        -tanThetaH2 * (
            (TdotH * TdotH) / (alphaX * alphaX) +
            (BdotH * BdotH) / (alphaY * alphaY)
        );

    float denom = 4.0 * PI * alphaX * alphaY * sqrt(max(NdotL * NdotV, 1e-6));

    return exp(exponent) / denom;
}

float fd_orennayar(vec3 N, vec3 L, vec3 V, float roughness) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    if (NdotL <= 0.0 || NdotV <= 0.0)
        return 0;

    float sigma = roughness * roughness;

    float sigma2 = sigma * sigma;

    float A = 1.0 - (0.5 * sigma2 / (sigma2 + 0.33));
    float B = 0.45 * sigma2 / (sigma2 + 0.09);

    float theta_i = acos(NdotL);
    float theta_r = acos(NdotV);

    float alpha = max(theta_i, theta_r);
    float beta  = min(theta_i, theta_r);

    vec3 Vperp = normalize(V - N * NdotV);
    vec3 Lperp = normalize(L - N * NdotL);

    float cos_phi_diff = max(dot(Vperp, Lperp), 0.0);

    return (A + B * cos_phi_diff * sin(alpha) * tan(beta));

    
}

float fd_burley(vec3 N, vec3 V, vec3 H, vec3 L, float roughness) {
    float vdoth = max(0, dot(V,H));
    float ndotv = max(0, dot(N,V));
    float ndotl = max(0, dot(N,L));
    float F90_1 = -0.5 + (2 * roughness * vdoth * vdoth);

    return (1 + (F90_1 * pow(1-ndotl, 5))) * (1 + (F90_1 * pow(1-ndotv, 5)));
}


vec3 toneMapACES(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b)) / (x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 l;
    if (scene.lightPos.w == 1.0)
        l = normalize(scene.lightPos.xyz - worldPosition);
    else
        l = normalize(-scene.lightPos.xyz);

    vec3 v = normalize(scene.camPos - worldPosition);
    vec3 h = normalize(v+l);

    
    vec3 n = normalize(worldNormal);
    vec3 t = normalize(worldTangent);
    vec3 b = normalize(cross(n, t));
    mat3 TBN = mat3(t, b, n);

    vec3 normal = texture(normalMap, uv).rgb;
    // normal = normal * 2.0 - 1.0;

    n = ((scene.textures & TEXTURE_NORMAL_MAP) != 0)
        ? normalize(TBN * normal)
        : n;


    vec3 albedo = ((scene.textures & TEXTURE_ALBEDO) != 0)
        ? texture(tex, uv).rgb
        : material.albedo;
    
    float roughness = ((scene.textures & TEXTURE_ROUGHNESS_MAP) != 0)
        ? texture(roughnessMap, uv).r
        : material.roughness;

    float metallic = ((scene.textures & TEXTURE_METALLIC_MAP) != 0)
        ? texture(metallicMap, uv).r
        : material.metallic;

    vec3 color = vec3(0,0,0);
    float ndotl = max(dot(normalize(n), l), 0.0);
    float ndotv = max(dot(normalize(n), v), 0.0);
    if (scene.ibl) {
        float kD = 1.0 - metallic;
        vec3 R = reflect(-v,n);
        if ((BRDF_DIFFUSE_LAMBERT & BRDF) != 0) {
            color += texture(irradiance, n).rgb * albedo * kD;
        }

        if ((BRDF_DIFFUSE_OREN_NAYAR & BRDF) != 0) {
            color += texture(irradiance, n).rgb * albedo * fd_orennayar(n, n, v, roughness) * kD;
        }

        if ((BRDF_DIFFUSE_BURLEY & BRDF) != 0) {
            color += texture(irradiance, n).rgb * albedo * fd_burley(n, v, normalize(v+n), n, roughness) * kD;
        }

        if ((BRDF_SPECULAR_BLINN_PHONG & BRDF) != 0) {
            color += samplePrefilter(R, roughness) * fs_blinnphong(n, n, roughness) * .001f * albedo;
        }

        if ((BRDF_SPECULAR_COOK_TORRANCE & BRDF) != 0) {
            vec3 prefilteredColor = samplePrefilter(R, roughness);
            vec2 brdf = texture(brdfLUT, vec2(ndotv, roughness)).rg;
            vec3 F0 = mix(vec3(0.04), albedo, metallic);
            color += prefilteredColor * (F0 * brdf.x + brdf.y);
        
        }

        if ((BRDF_SPECULAR_WARD & BRDF) != 0) {
            vec3 Rt = vec3(
                dot(R, t),
                dot(R, b),
                dot(R, n)
            );

            Rt.y *= 1;
            Rt.x *= roughness;

            vec3 R_aniso = normalize(
                Rt.x * t +
                Rt.y * b +
                Rt.z * n
            );
            vec3 envSpec = samplePrefilter(R_aniso, roughness);
            vec3 H = normalize(v + R_aniso);
            vec3 F = F_schlick(v, H, metallic, albedo);

            color += envSpec * F;
        }

        if ((BRDF_SPECULAR_DISNEY & BRDF) != 0) {
            vec3 prefilteredColor = samplePrefilter(R, roughness);
            vec2 brdf = texture(brdfLUT, vec2(ndotv, roughness)).rg;
            vec3 F0 = mix(vec3(0.04), albedo, metallic);
            color += prefilteredColor * (F0 * brdf.x + brdf.y);

            // CLEARCOAT
            float ccRoughness = 1-material.clearcoatGloss;
            vec3 prefilteredCC = samplePrefilter(R, ccRoughness);
            
            float Fc = pow(1.0 - ndotv, 5.0);
            float Fcc = mix(0.04, 1.0, Fc);
            vec2 brdfcc = texture(brdfLUT, vec2(ndotv, ccRoughness)).rg;
            color += material.clearcoat * prefilteredCC * (Fcc * brdfcc.x + brdfcc.y);

            // SHEEN
            vec3 sheenColor = mix(normalize(vec3(1.0)), albedo, material.sheenTint);
            if (length(sheenColor) == 0) sheenColor = vec3(1.0);
            sheenColor = normalize(sheenColor);
            color += material.sheen * sheenColor * Fc * texture(irradiance, n).rgb;
        }



    }
    else {
        color = scene.ambientLight * albedo * 0.1; 
        vec3 kD = vec3(1.0) - F_schlick(v, h, metallic, albedo);
        kD *= (1.0 - metallic);

        if ((BRDF_DIFFUSE_LAMBERT & BRDF) != 0) {
            color += (albedo/PI) * ndotl * scene.lightColor * kD;
        }

        if ((BRDF_DIFFUSE_OREN_NAYAR & BRDF) != 0) {
            color += (albedo/PI) * fd_orennayar(n, l, v, roughness) * ndotl * scene.lightColor * kD;
        }

        if ((BRDF_DIFFUSE_BURLEY & BRDF) != 0) {
            color += (albedo/PI) * fd_burley(n, v, h, l, roughness) * ndotl * scene.lightColor * kD;
        }

        if ((BRDF_SPECULAR_BLINN_PHONG & BRDF) != 0) {
            color += fs_blinnphong(h, n, roughness) * ndotl * scene.lightColor * .1;
        }

        if ((BRDF_SPECULAR_COOK_TORRANCE & BRDF) != 0) {
            color += fs_CookTorrance(v, l, h, n, albedo, metallic, roughness) * ndotl * scene.lightColor;
        }

        if ((BRDF_SPECULAR_WARD & BRDF) != 0) {
            color += F_schlick(v, h, metallic, albedo) * fs_ward(n, v, l , t, b, 0.1, roughness) * ndotl * scene.lightColor;
        }

        if ((BRDF_SPECULAR_DISNEY & BRDF) != 0) {
            color += fs_CookTorrance(v, l, h, n, albedo, metallic, roughness) * ndotl * scene.lightColor;
            color += f_clearcoat(n, v, l, h, roughness);
            color += f_sheen(n, l, h, albedo);
        }

    }

    
    if (scene.toneMapping) {
        color *= scene.exposure;
        color = toneMapACES(color);
    }

    outColor = vec4(color, 1.0);
    // outColor = vec4(n*0.5 + vec3(0.5), 1.0);
    // outColor = vec4(vec3(roughness), 1.0);
    // outColor = vec4(vec3(metallic), 1.0);
}